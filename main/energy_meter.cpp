#include "energy_meter.h"

#include <driver/gpio.h>
#include <esp_log.h>

#include "KMPProDinoESP32.h"

static const char* TAG = "energy_meter";

esp_err_t EnergyMeter::begin() {
    ESP_LOGI(TAG, "EnergyMeter::begin");

    int err = xTaskCreatePinnedToCore(&task, "energyMeterTask", 6 * 1024, this, 11, &taskHandle, 1);
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
"object_id":"nibegw-energy-meter",
"unique_id":"nibegw-energy-meter",
"name":"Nibe Energy Meter",
"state_topic":"%s",
"unit_of_measurement":"kWh",
"device_class":"energy",
"state_class":"total_increasing:,
%s
})";

esp_err_t EnergyMeter::beginMqtt(MqttClient& mqttClient) {
    ESP_LOGI(TAG, "EnergyMeter::beginMqtt");
    this->mqttClient = &mqttClient;

    mqttTopic = mqttClient.getConfig().rootTopic + "/energy-meter";
    // announce energy meter
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
        xTaskNotifyWait(0, 0, 0, portMAX_DELAY);

        meter->energyInWh++;
        ESP_LOGI(TAG, "EnergyMeter::task: isr=%lu, task=%lu", meter->pulseCounterISR, meter->energyInWh);

        // wait 110ms (S0 impulse is 90ms according to spec)
        vTaskDelay(110 / portTICK_PERIOD_MS);
        // reset interrupt by reading GPIO register
        MCP23S08.GetPinState();
    }
}

esp_err_t EnergyMeter::publishState() {
    ESP_LOGI(TAG, "EnergyMeter::publishState: isr=%lu, task=%lu", pulseCounterISR, energyInWh);
    // TODO: move to util class, tests
    auto s = std::to_string(energyInWh / 1000);
    auto remainder = energyInWh % 1000;
    if (remainder < 10) {
        s += ".00";
    } else if (remainder < 100) {
        s += ".0";
    } else {
        s += ".";
    }
    s += std::to_string(remainder);

    mqttClient->publish(mqttTopic, s);
    return ESP_OK;
}