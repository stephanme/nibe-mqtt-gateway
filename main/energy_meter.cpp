#include "energy_meter.h"

#include <driver/gpio.h>
#include <esp_log.h>

#include "KMPProDinoESP32.h"
#include "MCP23S08.h"
#include "config.h"

static const char* TAG = "energy_meter";

EnergyMeter::EnergyMeter(Metrics& metrics)
    : metrics(metrics),
      metricEnergyInWh(metrics.addMetric("nibe_energy_meter_wh_total", 1, 1, true)),
      metricEnergyConsumptionInWhUnknown(metrics.addMetric("nibe_energy_consumption_wh_total{mode=\"unknown\"}", 1, 1, true)),
      metricEnergyConsumptionInWhOff(metrics.addMetric("nibe_energy_consumption_wh_total{mode=\"off\"}", 1, 1, true)),
      metricEnergyConsumptionInWhHeating(metrics.addMetric("nibe_energy_consumption_wh_total{mode=\"heating\"}", 1, 1, true)),
      metricEnergyConsumptionInWhHotwater(metrics.addMetric("nibe_energy_consumption_wh_total{mode=\"hotwater\"}", 1, 1, true)),
      metricEnergyConsumptionInWhCooling(metrics.addMetric("nibe_energy_consumption_wh_total{mode=\"cooling\"}", 1, 1, true)) {}

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

    // reset energy consumption metrics
    metricEnergyConsumptionInWhUnknown.setValue(0);
    metricEnergyConsumptionInWhOff.setValue(0);
    metricEnergyConsumptionInWhHeating.setValue(0);
    metricEnergyConsumptionInWhHotwater.setValue(0);
    metricEnergyConsumptionInWhCooling.setValue(0);

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

    // configure MCP23S08C interrupt for OptoIn1, interrupt-on-pin-change (INTCON=0)
    MCP23S08.GetPinState();  // clears any old pending interrupt, TODO: is this really safe?
    MCP23S08.ConfigureInterrupt(OPTO_IN_1_PIN, true, false, false);

    ESP_LOGI(TAG, "init from NVS: %lu", lastStoredEnergyInWh);
    return err;
}

esp_err_t EnergyMeter::beginMqtt(MqttClient& mqttClient) {
    ESP_LOGI(TAG, "EnergyMeter::beginMqtt");
    this->mqttClient = &mqttClient;

    mqttTopic = mqttClient.getConfig().rootTopic + "/energy-meter";
    // announce energy meter, use full device info
    JsonDocument deviceDiscovery = mqttClient.getDeviceDiscoveryInfo();
    deviceDiscovery["name"] = "Nibe Energy Meter";
    deviceDiscovery["obj_id"] = "nibegw-energy-meter";
    deviceDiscovery["uniq_id"] = "nibegw-energy-meter";
    deviceDiscovery["stat_t"] = mqttTopic;
    deviceDiscovery["unit_of_meas"] = "kWh";
    deviceDiscovery["dev_cla"] = "energy";
    deviceDiscovery["stat_cla"] = "total_increasing";

    std::string discoveryPayload;
    serializeJson(deviceDiscovery, discoveryPayload);
    mqttClient.publish(mqttClient.getConfig().discoveryPrefix + "/sensor/nibegw/energy-meter/config", discoveryPayload, QOS0,
                       true);

    return ESP_OK;
}

// interrupt-on-pin-change -> 2 interrupts for every S0 pulse
// (S0 impulse is 90ms according to meter spec, max freq is 3.3/s for 12kW -> 300ms is shortest time between pulses)
// notify EnergyMeter::task which evaluates pin state and counts energy, no SPI communication in ISR
void IRAM_ATTR EnergyMeter::gpio_interrupt_handler(void* args) {
    EnergyMeter* meter = (EnergyMeter*)args;
    meter->isrCounter++;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(meter->taskHandle, 0, eNoAction, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void EnergyMeter::task(void* pvParameters) {
    EnergyMeter* meter = (EnergyMeter*)pvParameters;
    while (1) {
        xTaskNotifyWait(0, ULONG_MAX, 0, portMAX_DELAY);

        // read INTCAP (state at interrupt time), resets interrupt
        // 0 = start of S0 pulse
        bool intcapState = MCP23S08.GetInterruptCaptureState(OPTO_IN_1_PIN);
        if (!intcapState) {
            u_int32_t energyInWh;
            if (meter->skipNextPulses > 0) {
                // adjust energyInWh by skipping pulses
                // counter metric must not decrease
                meter->skipNextPulses--;
                energyInWh = meter->metricEnergyInWh.getValue();
            } else {
                // regular operation
                energyInWh = meter->metricEnergyInWh.incrementValue(1);
                meter->countConsumedEnergyPerOperationMode();
            }
            ESP_LOGV(TAG, "EnergyMeter::task: isrCounter=%lu, energyInWh=%lu", meter->isrCounter, energyInWh);
        }
    }
}

void EnergyMeter::countConsumedEnergyPerOperationMode() {
    int mode = 0;
    if (metricNibeOperationMode == nullptr) {
        // TODO: hardcode metric name, might want to make this configurable
        metricNibeOperationMode = metrics.findMetric("nibe_operation_mode{register=\"43086\"}");
        if (metricNibeOperationMode != nullptr) {
            mode = metricNibeOperationMode->getValue();
        }
    } else {
        mode = metricNibeOperationMode->getValue();
    }
    // nibe register 43086: prio = operation mode, 10=Off 20=Hot Water 30=Heat 40=Pool 41=Pool 2 50=Transfer 60=Cooling
    switch (mode) {
        case 10:
            metricEnergyConsumptionInWhOff.incrementValue(1);
            break;
        case 20:
            metricEnergyConsumptionInWhHotwater.incrementValue(1);
            break;
        case 30:
            metricEnergyConsumptionInWhHeating.incrementValue(1);
            break;
        case 60:
            metricEnergyConsumptionInWhCooling.incrementValue(1);
            break;
        default:
            // Unknown = nibe_operation_mode metric not found (yet)
            // includes Pool and Transfer which is not used normally
            metricEnergyConsumptionInWhUnknown.incrementValue(1);
            break;
    }
}

esp_err_t EnergyMeter::publishState() {
    // skip if not initialized (e.g. when booting in safe mode)
    if (mqttClient == nullptr) {
        return ESP_FAIL;
    }

    auto energyInWh = metricEnergyInWh.getValue();
    ESP_LOGD(TAG, "EnergyMeter::publishState: isrCounter=%lu, energyInWh=%lu", isrCounter, energyInWh);

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