#include "nibegw_config.h"

#include <esp_log.h>

#include <bit>

static const char* TAG = "nibegw_config";

std::string Coil::decodeCoilData(const NibeReadResponseData& data) const {
    std::string value;
    switch (dataType) {
        case COIL_DATA_TYPE_UINT8:
            value = formatNumber(data.value[0]);
            break;
        case COIL_DATA_TYPE_INT8:
            value = formatNumber((int8_t)data.value[0]);
            break;
        case COIL_DATA_TYPE_UINT16:
            value = formatNumber(std::byteswap(*(uint16_t*)data.value));
            break;
        case COIL_DATA_TYPE_INT16:
            value = formatNumber(std::byteswap(*(int16_t*)data.value));
            break;
        case COIL_DATA_TYPE_UINT32:
            value = formatNumber(std::byteswap(*(uint32_t*)data.value));
            break;
        case COIL_DATA_TYPE_INT32:
            value = formatNumber(std::byteswap(*(int32_t*)data.value));
            break;
        default:
            ESP_LOGW(TAG, "Coil %d has unknown data type %d", id, dataType);
            break;
    }
    return value;
}
