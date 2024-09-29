#ifndef _energy_meter_h_
#define _energy_meter_h_

#include <nvs.h>

#include "KMPProDinoESP32.h"
#include "metrics.h"
#include "mqtt.h"

#define ENERGY_METER_TASK_PRIORITY 11

// OPTOIN_PINS[OptoIn1] = 3 (not exposed by KMPProDinoESP32, abstraction is broken)
#define OPTO_IN_1_PIN 3

#define NIBEGW_NVS_KEY_ENERGY_IN_WH "energyInWh"

// Uses OptoIn1 to read S0 interface of an energy meter (e.g. DRT-428D).
// DRT-428D spec: 1000 impulses/kWh, impulse length 90ms.
// -> max impulses: 3x 20A * 230V / 3600s/h = 3.833 impulses/s = 1 impulse every ~260ms
// S0 pulse = 0 on input pin
// S0 impulse counting is based on MCP23S08C interrupts (on GPIO36)
// INTCON: interrupt-on-pin-change, i.e. 2 interrupts for every S0 pulse
// INTCAP=0 means start of S0 pulse (and resets interrupt) -> increment energy by 1 Wh
// Attention: interrupt is also reset by reading pin stage (GPIO) which happens on relay state publishing
class EnergyMeter {
   public:
    EnergyMeter(Metrics& metrics);

    esp_err_t begin();
    esp_err_t beginMqtt(MqttClient& mqttClient);
    esp_err_t publishState();

    u_int32_t getEnergyInWh() { return metricEnergyInWh.getValue(); }
    // for adjusting this energy meter with the real meter
    void setEnergyInWh(u_int32_t energyInWh) { metricEnergyInWh.setValue(energyInWh); }
    void adjustEnergyInWh(u_int32_t energyInWh);

   private:
    nvs_handle_t nvsHandle;

    static void IRAM_ATTR gpio_interrupt_handler(void* args);
    static void task(void* pvParameters);
    void countConsumedEnergyPerOperationMode();

    TaskHandle_t taskHandle;
    u_int32_t isrCounter = 0;
    u_int32_t lastStoredEnergyInWh = 0;

    Metrics& metrics;
    // energy meter, absolute value, stored in NVS
    Metric& metricEnergyInWh;
    int skipNextPulses = 0;
    // consumed energy, per Nibe operation mode, not stored in NVS
    Metric& metricEnergyConsumptionInWhUnknown;
    Metric& metricEnergyConsumptionInWhOff;
    Metric& metricEnergyConsumptionInWhHeating;
    Metric& metricEnergyConsumptionInWhHotwater;
    Metric& metricEnergyConsumptionInWhCooling;
    Metric* metricNibeOperationMode = nullptr;

    MqttClient* mqttClient = nullptr;
    std::string mqttTopic;
};

#endif