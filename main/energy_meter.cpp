#include "energy_meter.h"

#include <driver/gpio.h>
#include <esp_log.h>

#include "KMPProDinoESP32.h"
#include "config.h"

static const char* TAG = "energy_meter";

esp_err_t EnergyMeter::begin() {
    ESP_LOGI(TAG, "begin");

    // read persisted energyInWh value
    int err;
    err = nvs_open(NIBEGW_NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %d", err);
        return err;
    }
    err = nvs_get_u32(nvsHandle, NIBEGW_NVS_KEY_ENERGY_IN_WH, &lastStoredEnergyInWh);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "nvs_get_u32: key %s not found", NIBEGW_NVS_KEY_ENERGY_IN_WH);
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_get_u32(%s)  failed: %d", NIBEGW_NVS_KEY_ENERGY_IN_WH, err);
        return err;
    }
    metricEnergyInWh.setValue(lastStoredEnergyInWh);

    err = xTaskCreatePinnedToCore(&task, "energyMeterTask", 6 * 1024, this, ENERGY_METER_TASK_PRIORITY, &taskHandle, 1);
    if (err != pdPASS) {
        ESP_LOGE(TAG, "Could not start energyMeterTask task");
        return ESP_FAIL;
    }

    // configure interrupts and install interrupt handler
    // GPIO_NUM_36 = MCP23S08InterruptPin
    err = gpio_set_intr_type(GPIO_NUM_36, GPIO_INTR_NEGEDGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_set_intr_type failed: %d", err);
        return err;
    }

    // TODO: interrupt pin belongs to MCP23S08, not to EnergyMeter - broken abstraction
    // assumes that GPIO isr service already installed
    err = gpio_isr_handler_add(GPIO_NUM_36, gpio_interrupt_handler, (void*)this);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add failed: %d", err);
        return err;
    }
    err = gpio_intr_enable(GPIO_NUM_36);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_intr_enable failed: %d", err);
        return err;
    }

    // configure MCP23S08C interrupt for OptoIn1
    // OPTOIN_PINS[OptoIn1] = 3 (not exposed by KMPProDinoESP32, abstraction is broken)
    MCP23S08.GetPinState();  // clears any old pending interrupt, TODO: is this really safe?
    MCP23S08.ConfigureInterrupt(3, true, false, true);

    return err;
}

static const char* DISCOVERY_PAYLOAD = R"({
"obj_id":"nibegw-energy-meter",
"uniq_id":"nibegw-energy-meter",
"name":"Nibe Energy Meter",
"stat_t":"%s",
"unit_of_meas":"kWh",
"dev_cla":"energy",
"stat_cla":"total_increasing",
%s
})";

esp_err_t EnergyMeter::beginMqtt(MqttClient& mqttClient) {
    ESP_LOGI(TAG, "EnergyMeter::beginMqtt");
    this->mqttClient = &mqttClient;

    mqttTopic = mqttClient.getConfig().rootTopic + "/energy-meter";
    // announce energy meter, use full device info
    char discoveryPayload[strlen(DISCOVERY_PAYLOAD) + mqttTopic.length() + mqttClient.getDeviceDiscoveryInfo().length() + 1];
    snprintf(discoveryPayload, sizeof(discoveryPayload), DISCOVERY_PAYLOAD, mqttTopic.c_str(),
             mqttClient.getDeviceDiscoveryInfo().c_str());
    mqttClient.publish(mqttClient.getConfig().discoveryPrefix + "/sensor/nibegw/energy-meter/config", discoveryPayload, QOS0,
                       true);

    return ESP_OK;
}

void IRAM_ATTR EnergyMeter::gpio_interrupt_handler(void* args) {
    EnergyMeter* meter = (EnergyMeter*)args;
    meter->pulseCounterISR++;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(meter->taskHandle, 0, eNoAction, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void EnergyMeter::task(void* pvParameters) {
    EnergyMeter* meter = (EnergyMeter*)pvParameters;
    while (1) {
        xTaskNotifyWait(0, ULONG_MAX, 0, portMAX_DELAY);

        u_int32_t energyInWh;
        if (meter->skipNextPulses > 0) {
            // adjust energyInWh by skipping pulses
            // counter metric must not decrease
            meter->skipNextPulses--;
            energyInWh = meter->metricEnergyInWh.getValue();
        } else {
            // regular operation
            energyInWh = meter->metricEnergyInWh.incrementValue(1);
        }
        ESP_LOGV(TAG, "EnergyMeter::task: isr=%lu, task=%lu", meter->pulseCounterISR, energyInWh);

        // wait 150ms (S0 impulse is 90ms according to spec, max freq is 3.3/s for 12kW -> 300ms is shortest time between pulses)
        vTaskDelay(150 / portTICK_PERIOD_MS);
        // reset interrupt by reading GPIO register
        MCP23S08.GetPinState();
    }
}

esp_err_t EnergyMeter::publishState() {
    auto energyInWh = metricEnergyInWh.getValue();
    ESP_LOGD(TAG, "EnergyMeter::publishState: isr=%lu, task=%lu", pulseCounterISR, energyInWh);

    // store in nvs if changed
    if (energyInWh != lastStoredEnergyInWh) {
        lastStoredEnergyInWh = energyInWh;
        int err = nvs_set_u32(nvsHandle, NIBEGW_NVS_KEY_ENERGY_IN_WH, energyInWh);
        if (err == ESP_OK) {
            err = nvs_commit(nvsHandle);
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "nvs_set_u32/nvs_commit(%s) failed: %d", NIBEGW_NVS_KEY_ENERGY_IN_WH, err);
        }
    }

    // mqtt: report in kWh
    auto s = Metrics::formatNumber(energyInWh, 1000, 1);
    mqttClient->publish(mqttTopic, s);
    return ESP_OK;
}

// smooth adjustment of energy counter
// allow for max 10 kWh change, never count backwards
void EnergyMeter::adjustEnergyInWh(u_int32_t energyInWh) {
    u_int32_t currentEnergyInWh = metricEnergyInWh.getValue();
    int diff = energyInWh - currentEnergyInWh;
    if (std::abs(diff) > 10000) {
        ESP_LOGW(TAG, "adjustEnergyInWh: diff=%d, too large (max 10kWh), skipping", diff);
        return;
    }
    ESP_LOGI(TAG, "adjustEnergyInWh: change from %lu to %lu, diff=%d", currentEnergyInWh, energyInWh, diff);
    if (diff >= 0) {
        setEnergyInWh(energyInWh);
    } else {
        skipNextPulses = -diff;
    }
}