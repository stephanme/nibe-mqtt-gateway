// KMPProDinoESP32.cpp
// Company: KMP Electronics Ltd, Bulgaria
// Web: https://kmpelectronics.eu/
// Supported boards:
//		KMP ProDino ESP32 V1 https://kmpelectronics.eu/products/prodino-esp32-v1/
//		KMP ProDino ESP32 Ethernet V1 https://kmpelectronics.eu/products/prodino-esp32-ethernet-v1/
//		KMP ProDino ESP32 GSM V1 https://kmpelectronics.eu/products/prodino-esp32-gsm-v1/
//		KMP ProDino ESP32 LoRa V1 https://kmpelectronics.eu/products/prodino-esp32-lora-v1/
//		KMP ProDino ESP32 LoRa RFM V1 https://kmpelectronics.eu/products/prodino-esp32-lora-rfm-v1/
//		KMP ProDino ESP32 Ethernet GSM V1 https://kmpelectronics.eu/products/prodino-esp32-ethernet-gsm-v1/
//		KMP ProDino ESP32 Ethernet LoRa RFM V1 https://kmpelectronics.eu/products/prodino-esp32-ethernet-lora-rfm-v1/
//		KMP ProDino ESP32 Ethernet LoRa V1 https://kmpelectronics.eu/products/prodino-esp32-ethernet-lora-v1/
// Description:
//		Source file for KMP Dino WiFi ESP32 board.
// Version: 0.6.5
// Date: 20.12.2018
// Author: Plamen Kovandjiev <p.kovandiev@kmpelectronics.eu> & Dimitar Antonov <d.antonov@kmpelectronics.eu>

// requires arduino-esp >=3.0.0-rc3

#include "KMPProDinoESP32.h"

#include <ETH.h>
#include <SPI.h>
#include <esp_log.h>
#include <esp_netif_sntp.h>

#include "MCP23S08.h"
#include "esp32-hal-rgb-led.h"

static const char* TAG = "prodino";

struct BoardConfig_t {
    BoardType Board;
    bool Ethernet;
    bool GSM;
    bool LoRa;
    bool LoRaRFM95;
};

const BoardConfig_t BoardConfig[BOARDS_COUNT] = {
    {ProDino_ESP32, false, false, false, false}, {ProDino_ESP32_Ethernet, true, false, false, false},
    // {ProDino_ESP32_GSM, false, true, false, false},
    // {ProDino_ESP32_LoRa, false, false, true, false},
    // {ProDino_ESP32_LoRa_RFM, false, false, false, true},
    // {ProDino_ESP32_Ethernet_GSM, true, true, false, false},
    // {ProDino_ESP32_Ethernet_LoRa, true, false, true, false},
    // {ProDino_ESP32_Ethernet_LoRa_RFM, true, false, false, true},
};

// Relay pins
#define REL1PIN 7
#define REL2PIN 6
#define REL3PIN 5
#define REL4PIN 4
/**
 * @brief Relay pins.
 */
const uint8_t RELAY_PINS[RELAY_COUNT] = {REL1PIN, REL2PIN, REL3PIN, REL4PIN};

// Input pins
#define IN1PIN 3
#define IN2PIN 2
#define IN3PIN 1
#define IN4PIN 0
/**
 * @brief Input pins.
 */
const int OPTOIN_PINS[OPTOIN_COUNT] = {IN1PIN, IN2PIN, IN3PIN, IN4PIN};

// Expander CS pin.
#define MCP23S08CSPin 32  // IO32

// Status RGB LED.
#define StatusLedPixelCount 1
#define StatusLedPin 0
#define StatusLedPixelNumber 0

// SPI pins.
#define SPI_SCK 18
#define SPI_MISO 19
#define SPI_MOSI 23

// W5500 pins.
#define W5500ResetPin 12  // I12
#define W5500CSPin 33     // IO33

// RS485 pins. Serial1.
#define RS485Pin 2     // IO12
#define RS485RxPin 4   // IO4
#define RS485TxPin 16  // IO16
#define RS485Transmit HIGH
#define RS485Receive LOW
HardwareSerial RS485Serial(1);

// GSM module pins for Serial2
#define GSMCTSPin J14_5
#define GSMRTSPin J14_6
#define GSMRxPin J14_7
#define GSMTxPin J14_8
#define GSMDTRPin J14_9
#define GSMResetPin J14_10

// LoRa module pins
#define LoRaRxPin J14_5
#define LoRaLowPin J14_6
#define LoRaTxPin J14_8
#define LoRaBootPin J14_11
#define LoRaResetPin J14_12

// RFM module pins
// J14_2	5V
// J14_3	GND // RFM95_GND
// J14_4	3V3 // RFM95_3V3
// J14_5	X
#define RFM95_DIO2 J14_6
// J14_7	X
// J14_8	25 // DIO1
// J14_9	26 // DIO0
#define RFM95_RESET J14_10
// J14_11	X
// J14_12	X
#define RFM95_SCK J2_1
#define RFM95_MISO J2_2
#define RFM95_MOSI J2_3
#define RFM95_NSS J2_4
// J2_5	1  // IO5
// J2_6 GND
HardwareSerial SerialModem(2);

#define colorSaturation 20  // Max 255 but light is too sharp.
#define COLOR(r, g, b) (((uint32_t)r << 16) | ((uint32_t)g << 8) | b)

uint32_t yellow = COLOR(colorSaturation, colorSaturation, 0);
uint32_t orange = COLOR(colorSaturation, colorSaturation / 2, 0);
uint32_t red = COLOR(colorSaturation, 0, 0);
uint32_t green = COLOR(0, colorSaturation, 0);
uint32_t blue = COLOR(0, 0, colorSaturation);
uint32_t white = COLOR(colorSaturation, colorSaturation, colorSaturation);
uint32_t black = COLOR(0, 0, 0);

uint32_t _statusLedColor;

KMPProDinoESP32Class KMPProDinoESP32;
BoardType _board;

uint32_t _TxFlushDelayuS;

unsigned long _blinkIntervalTimeout = 0;
bool _ledState = false;

static bool eth_connected = false;

void _network_onEvent(arduino_event_id_t event, arduino_event_info_t info) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            ESP_LOGI(TAG, "ETH Started");
            ESP_LOGI(TAG, "ETH MAC: %s", ETH.macAddress().c_str());
            ESP_LOGI(TAG, "ETH hostname: %s", ETH.getHostname());
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            ESP_LOGI(TAG, "ETH Connected");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            eth_connected = true;
            ESP_LOGI(TAG, "ETH Got IP: %s", esp_netif_get_desc(info.got_ip.esp_netif));
            ESP_LOGI(TAG, "IP:  %s", ETH.localIP().toString().c_str());
            ESP_LOGI(TAG, "DNS: %s", ETH.dnsIP().toString().c_str());
            esp_netif_sntp_start();
            break;
        case ARDUINO_EVENT_ETH_LOST_IP:
            ESP_LOGI(TAG, "ETH Lost IP");
            eth_connected = false;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            ESP_LOGI(TAG, "ETH Disconnected");
            eth_connected = false;
            break;
        case ARDUINO_EVENT_ETH_STOP:
            ESP_LOGI(TAG, "ETH Stopped");
            eth_connected = false;
            break;
        default:
            break;
    }
}

void KMPProDinoESP32Class::begin(BoardType board) { begin(board, true, true); }

void KMPProDinoESP32Class::begin(BoardType board, bool startEthernet, bool startModem) {
    ESP_LOGI(TAG, "Initializing board");
    _board = board;

    bool isBoardInitialized = false;

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

    for (size_t i = 0; i < BOARDS_COUNT; i++) {
        BoardConfig_t boardConfig = BoardConfig[i];
        if (board == boardConfig.Board) {
            isBoardInitialized = true;

            if (boardConfig.Ethernet) {
                beginEthernet(startEthernet);
            }

            if (boardConfig.GSM) {
                beginGSM(startModem);
            }

            if (boardConfig.LoRa) {
                beginLoRa(startModem);
            }

            if (boardConfig.LoRaRFM95) {
                beginLoRaRFM95(startModem);
            }

            break;
        }
    }

    if (!isBoardInitialized) {
        ESP_LOGE(TAG, "The board is not initialized!");
        while (1) {
        }
    }

    // Init expander pins.
    pinMode(MCP23S08InterruptPin, INPUT);

    // Set expander pins direction.
    MCP23S08.init(MCP23S08CSPin);
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        MCP23S08.SetPinDirection(RELAY_PINS[i], OUTPUT);
    }

    for (uint8_t i = 0; i < OPTOIN_COUNT; i++) {
        MCP23S08.SetPinDirection(OPTOIN_PINS[i], INPUT);
    }

    // Status led.
    //_pixel.begin();

    // RS485 pin init.
    pinMode(RS485Pin, OUTPUT);
    digitalWrite(RS485Pin, RS485Receive);

    ESP_LOGI(TAG, "Board initialized");
}

void KMPProDinoESP32Class::beginEthernet(bool startEthernet) {
    // To be called before ETH.begin()
    // esp_read_mac(_mac, ESP_MAC_ETH);
    Network.onEvent(_network_onEvent);

    // configure SNTP
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(0, {});
    config.start = false;            // start the SNTP service explicitly
    config.server_from_dhcp = true;  // accept the NTP offer from the DHCP server
    esp_netif_sntp_init(&config);

    // ProdDino W5500 is not connected to IRQ -> must configure polling mode
    if (!ETH.begin(ETH_PHY_W5500, -1, W5500CSPin, -1, W5500ResetPin, SPI, ETH_PHY_SPI_FREQ_MHZ)) {
        ESP_LOGE(TAG, "Failed to initialize Ethernet controller");
    }
}

void KMPProDinoESP32Class::beginGSM(bool startGSM) {
    // Start serial communication with the GSM modem.
    SerialModem.begin(115200, SERIAL_8N1, GSMRxPin, GSMTxPin);

    // Turn on the GSM module by triggering GSM_RESETN pin.
    pinMode(GSMResetPin, OUTPUT);
    if (startGSM) {
        restartGSM();
    } else {
        resetGSMOn();
    }

    // The GSM pin is output.
    pinMode(GSMCTSPin, INPUT);

    // RTS pin should be in LOW to the GSM modem works.
    pinMode(GSMRTSPin, OUTPUT);
    digitalWrite(GSMRTSPin, LOW);
}

void KMPProDinoESP32Class::beginLoRa(bool startLora) {
    // Start serial communication with the GSM modem.
    SerialModem.begin(19200, SERIAL_8N1, LoRaRxPin, LoRaTxPin);

    // Turn on the Lora module by triggering LoraResetPin pin.
    pinMode(LoRaResetPin, OUTPUT);
    if (startLora) {
        restartLoRa();
    } else {
        resetLoRaOn();
    }

    // LoRa low pin.
    pinMode(LoRaLowPin, OUTPUT);
    digitalWrite(LoRaLowPin, LOW);
}

void KMPProDinoESP32Class::restartLoRa() {
    // Reset occurs when a low level is applied to the RESET_N pin, which is normally set high by an internal pull-up, for a valid
    // time period min 10 mS.
    resetLoRaOn();
    delay(20);
    resetLoRaOff();
    delay(20);
}

void KMPProDinoESP32Class::resetLoRaOn() { digitalWrite(LoRaResetPin, LOW); }

void KMPProDinoESP32Class::resetLoRaOff() { digitalWrite(LoRaResetPin, HIGH); }

void KMPProDinoESP32Class::beginLoRaRFM95(bool startLoraRFM) {
    // Turn on the Lora module by triggering LoraResetPin pin.
    pinMode(RFM95_RESET, OUTPUT);
    if (startLoraRFM) {
        restartLoRaRFM95();
    } else {
        resetLoRaRFM95On();
    }

    // Set CS pin.
    pinMode(RFM95_NSS, OUTPUT);
    digitalWrite(RFM95_NSS, HIGH);
}

void KMPProDinoESP32Class::restartLoRaRFM95() {
    resetLoRaRFM95On();
    delay(20);
    resetLoRaRFM95Off();
    delay(20);
}

void KMPProDinoESP32Class::resetLoRaRFM95On() { digitalWrite(RFM95_RESET, LOW); }

void KMPProDinoESP32Class::resetLoRaRFM95Off() { digitalWrite(RFM95_RESET, HIGH); }

void KMPProDinoESP32Class::restartGSM() {
    // Reset occurs when a low level is applied to the RESET_N pin, which is normally set high by an internal pull-up, for a valid
    // time period min 10 mS. In our device this pin is inverted.
    resetGSMOn();
    delay(20);
    resetGSMOff();
}

void KMPProDinoESP32Class::resetGSMOn() { digitalWrite(GSMResetPin, HIGH); }

void KMPProDinoESP32Class::resetGSMOff() { digitalWrite(GSMResetPin, LOW); }

void KMPProDinoESP32Class::restartEthernet() {
    // RSTn Pull-up Reset (Active low) RESET should be held low at least 500 us for W5500 reset.
    digitalWrite(W5500ResetPin, LOW);
    delay(600);
    digitalWrite(W5500ResetPin, HIGH);
}

uint32_t KMPProDinoESP32Class::getStatusLed() { return _statusLedColor; }

void KMPProDinoESP32Class::setStatusLed(uint32_t color) {
    _statusLedColor = color;
    rgbLedWrite(StatusLedPixelNumber, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
}

// void KMPProDinoESP32Class::OnStatusLed()
//{
//	setStatusLed(true);
// }

void KMPProDinoESP32Class::offStatusLed() {
    _statusLedColor = black;
    rgbLedWrite(StatusLedPixelNumber, 0, 0, 0);
}

void KMPProDinoESP32Class::processStatusLed(uint32_t color, int blinkInterval) {
    if (millis() > _blinkIntervalTimeout) {
        _ledState = !_ledState;

        if (_ledState) {
            // Here you can check statuses: is WiFi connected, is there Ethernet connection and other...
            setStatusLed(color);
        } else {
            offStatusLed();
        }

        // Set next time to read data.
        _blinkIntervalTimeout = millis() + blinkInterval;
    }
}

// void KMPProDinoESP32Class::NotStatusLed()
//{
//	setStatusLed(!getStatusLed());
// }

/* ----------------------------------------------------------------------- */
/* Relays methods. */
/* ----------------------------------------------------------------------- */

void KMPProDinoESP32Class::setRelayState(uint8_t relayNumber, bool state) {
    // Check if relayNumber is out of range - return.
    if (relayNumber > RELAY_COUNT - 1) {
        return;
    }

    MCP23S08.SetPinState(RELAY_PINS[relayNumber], state);
}

void KMPProDinoESP32Class::setRelayState(Relay relay, bool state) { setRelayState((uint8_t)relay, state); }

void KMPProDinoESP32Class::setAllRelaysState(bool state) {
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        setRelayState(i, state);
    }
}

void KMPProDinoESP32Class::setAllRelaysOn() { setAllRelaysState(true); }

void KMPProDinoESP32Class::setAllRelaysOff() { setAllRelaysState(false); }

uint8_t KMPProDinoESP32Class::getRelayState(void) {
    uint8_t tState = (MCP23S08.GetPinState() & 0xf0) >> 4;
    uint8_t tRet = 0;
    if (tState & (1 << 0)) tRet |= 1 << 3;
    if (tState & (1 << 1)) tRet |= 1 << 2;
    if (tState & (1 << 2)) tRet |= 1 << 1;
    if (tState & (1 << 3)) tRet |= 1 << 0;
    return tRet;
}

bool KMPProDinoESP32Class::getRelayState(uint8_t relayNumber) {
    // Check if relayNumber is out of range - return false.
    if (relayNumber > RELAY_COUNT - 1) {
        return false;
    }

    return MCP23S08.GetPinState(RELAY_PINS[relayNumber]);
}

bool KMPProDinoESP32Class::getRelayState(Relay relay) { return getRelayState((uint8_t)relay); }

/* ----------------------------------------------------------------------- */
/* Opto input methods. */
/* ----------------------------------------------------------------------- */
uint8_t KMPProDinoESP32Class::getOptoInState(void) {
    uint8_t tState = ~MCP23S08.GetPinState();
    uint8_t tRet = 0;
    if (tState & (1 << 0)) tRet |= 1 << 3;
    if (tState & (1 << 1)) tRet |= 1 << 2;
    if (tState & (1 << 2)) tRet |= 1 << 1;
    if (tState & (1 << 3)) tRet |= 1 << 0;
    return tRet;
}

bool KMPProDinoESP32Class::getOptoInState(uint8_t optoInNumber) {
    // Check if optoInNumber is out of range - return false.
    if (optoInNumber > OPTOIN_COUNT - 1) {
        return false;
    }

    return !MCP23S08.GetPinState(OPTOIN_PINS[optoInNumber]);
}

bool KMPProDinoESP32Class::getOptoInState(OptoIn optoIn) { return getOptoInState((uint8_t)optoIn); }

/* ----------------------------------------------------------------------- */
/* RS485 methods. */
/* ----------------------------------------------------------------------- */
void KMPProDinoESP32Class::rs485Begin(unsigned long baud) { rs485Begin(baud, SERIAL_8N1); }

void KMPProDinoESP32Class::rs485Begin(unsigned long baud, uint32_t config) {
    RS485Serial.begin(baud, config, RS485RxPin, RS485TxPin);
    _TxFlushDelayuS = (uint32_t)((1000000 / baud) * 15);
}

void KMPProDinoESP32Class::rs485End() { RS485Serial.end(); }

/**
 * @brief Begin write data to RS485.
 *
 * @return void
 */
void KMPProDinoESP32Class::RS485BeginWrite() {
    digitalWrite(RS485Pin, RS485Transmit);
    // Allowing pin should delay for 50 nS
    delayMicroseconds(70);
}

/**
 * @brief End write data to RS485.
 *
 * @return void
 */
void KMPProDinoESP32Class::RS485EndWrite() {
    RS485Serial.flush();
    delayMicroseconds(_TxFlushDelayuS);
    digitalWrite(RS485Pin, RS485Receive);
}

size_t KMPProDinoESP32Class::rs485Write(const uint8_t data) {
    RS485BeginWrite();

    size_t result = RS485Serial.write(data);

    RS485EndWrite();

    return result;
}

size_t KMPProDinoESP32Class::rs485Write(const uint8_t* data, size_t dataLen) {
    RS485BeginWrite();

    size_t result = RS485Serial.write(data, dataLen);

    RS485EndWrite();

    return result;
}

int KMPProDinoESP32Class::rs485Read() { return rs485Read(10, 10); }

int KMPProDinoESP32Class::rs485Read(unsigned long delayWait, uint8_t repeatTime) {
    // If the buffer is empty, wait until the data arrives.
    while (!RS485Serial.available()) {
        delay(delayWait);
        --repeatTime;

        if (repeatTime == 0) {
            return -1;
        }
    }

    return RS485Serial.read();
}