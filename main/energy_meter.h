#ifndef _energy_meter_h_
#define _energy_meter_h_

#include "KMPProDinoESP32.h"
#include "mqtt.h"

// Uses OptoIn1 to read S0 interface of an energy meter (e.g. DRT-428D).
// DRT-428D spec: 1000 impulses/kWh, impulse length 90ms.
// -> max impulses: 3x 20A * 230V / 3600s/h = 3.833 impulses/s = 1 impulse every ~260ms
// S0 impulse counting is based on MCP23S08C interrupts (on GPIO36), every interrupt increments.
// This avoids any SPI communication with MCP23S08C but there can be only one S0 device attached.
class EnergyMeter {
   public:
    EnergyMeter() {}

    esp_err_t begin();
    esp_err_t beginMqtt(MqttClient& mqttClient);
    esp_err_t publishState();

    u_int32_t getEnergyInWh() { return energyInWh; }
    // for adjusting this energy meter with the real meter
    void setEnergyInWh(u_int32_t energyInWh) { this->energyInWh = energyInWh; }

   private:
    static void IRAM_ATTR gpio_interrupt_handler(void* args);
    static void task(void* pvParameters);

    TaskHandle_t taskHandle;
    u_int32_t pulseCounterISR = 0;
    u_int32_t energyInWh = 0;

    MqttClient* mqttClient;
    std::string mqttTopic;
};

#endif