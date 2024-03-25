#include "nibegw_rs485.h"

#include <esp_log.h>

static const char* TAG = "nibegw_rs485";

NibeRS485::NibeRS485(HardwareSerial* serial, int RS485DirectionPin, int RS485RxPin, int RS485TxPin) {
    connectionState = false;
    RS485 = serial;
    directionPin = RS485DirectionPin;
    this->RS485RxPin = RS485RxPin;
    this->RS485TxPin = RS485TxPin;
    pinMode(directionPin, OUTPUT);
    digitalWrite(directionPin, LOW);
}

esp_err_t NibeRS485::begin() {
    ESP_LOGI(TAG, "NibeRS485::begin");
    connect();
    return ESP_OK;
}

void NibeRS485::connect() {
    if (!connectionState) {
        RS485->begin(9600, SERIAL_8N1, RS485RxPin, RS485TxPin);
        connectionState = true;
    }
}

void NibeRS485::disconnect() {
    if (connectionState) {
        RS485->end();
        connectionState = false;
    }
}

boolean NibeRS485::isDataAvailable() { return RS485->available() > 0; }

int NibeRS485::readData() {
    if (RS485->available() > 0) {
        int b = RS485->read();
#if LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE
        if (b >= 0) {
            readLogBuffer[readLogIndex++] = b;
            if (readLogIndex >= sizeof(readLogBuffer)) {
                ESP_LOGV(TAG, "Rec: %s", NibeGw::dataToString(readLogBuffer, readLogIndex).c_str());
                readLogIndex = 0;
            }
        }
#endif
        return b;
    } else {
        return -1;
    }
}

void NibeRS485::sendData(const uint8_t* const data, uint8_t len) {
    digitalWrite(directionPin, HIGH);
    delay(1);
    RS485->write(data, len);
    RS485->flush();
    delay(1);
    digitalWrite(directionPin, LOW);

    ESP_LOGV(TAG, "Send %s", NibeGw::dataToString(data, len).c_str());
}

void NibeRS485::sendData(const uint8_t data) {
    digitalWrite(directionPin, HIGH);
    delay(1);
    RS485->write(data);
    RS485->flush();
    delay(1);
    digitalWrite(directionPin, LOW);

    ESP_LOGV(TAG, "Send %02x", data);
}
