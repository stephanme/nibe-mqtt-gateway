#include "KMPProDinoESP32.h"

KMPProDinoESP32Class KMPProDinoESP32;

bool KMPProDinoESP32Class::getRelayState(Relay relay) { return getRelayState((uint8_t)relay); }
bool KMPProDinoESP32Class::getRelayState(uint8_t relayNumber) { return false; }
void KMPProDinoESP32Class::setRelayState(uint8_t relayNumber, bool state) {}
void KMPProDinoESP32Class::setRelayState(Relay relay, bool state)  {}