// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef enums_h
#define enums_h

#include "bigpacks.h"

uint32_t enums_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);

enum board_model {
	BOARD_MODEL_NONE = 0,
	BOARD_MODEL_GENERIC,
	BOARD_MODEL_M5STACK_ATOM_LITE,
	BOARD_MODEL_M5STACK_ATOM_MATRIX,
	BOARD_MODEL_M5STACK_ATOM_ECHO,
	BOARD_MODEL_M5STACK_ATOM_U,
	BOARD_MODEL_M5STACK_ATOMS3,
	BOARD_MODEL_M5STACK_ATOMS3_LITE,
	BOARD_MODEL_M5STACK_M5STICKC,
	BOARD_MODEL_M5STACK_M5STICKC_PLUS,
	BOARD_MODEL_M5STACK_CORE2,
	BOARD_MODEL_M5STACK_CORE2_AWS,
	BOARD_MODEL_M5STACK_TOUGH,
	BOARD_MODEL_M5STACK_M5STATION_BAT,
	BOARD_MODEL_M5STACK_M5STATION_485,
	BOARD_MODEL_ADAFRUIT_ESP32_FEATHER_V2,
	BOARD_MODEL_ADAFRUIT_ESP32_S3_FEATHER,
	BOARD_MODEL_ADAFRUIT_QT_PY_ESP32_PICO,
	BOARD_MODEL_ADAFRUIT_QT_PY_ESP32_S3,
	BOARD_MODEL_SEEEDSTUDIO_XIAO_ESP32S3,
	BOARD_MODEL_SEEEDSTUDIO_XIAO_ESP32C3,
	BOARD_MODEL_NUM_MAX
};
extern const char *board_model_labels[];
typedef enum board_model board_model_t;

enum resource {
	RESOURCE_NONE = 0,
	RESOURCE_APPLICATION,
	RESOURCE_BOARD,
	RESOURCE_WIFI,
	RESOURCE_I2C,
	RESOURCE_ONEWIRE,
	RESOURCE_BLE,
	RESOURCE_ADC,
	RESOURCE_NUM_MAX
};
extern const char *resource_labels[];
typedef uint8_t resource_t;

enum backend_status {
	BACKEND_STATUS_OFFLINE = 0,
	BACKEND_STATUS_ONLINE,
	BACKEND_STATUS_ERROR,
	BACKEND_STATUS_NUM_MAX
};
extern const char *backend_status_labels[];
typedef enum backend_status backend_status_t;

enum backend_auth {
	BACKEND_AUTH_NONE = 0,
	BACKEND_AUTH_BASIC,
	BACKEND_AUTH_DIGEST,
	BACKEND_AUTH_BEARER,
	BACKEND_AUTH_TOKEN,
	BACKEND_AUTH_HEADER,
	BACKEND_AUTH_X509,
	BACKEND_AUTH_POSTMAN,
	BACKEND_AUTH_NUM_MAX
};
extern const char *backend_auth_labels[];
typedef enum backend_auth backend_auth_t;

enum backend_format {
	BACKEND_FORMAT_SENML = 0,
	BACKEND_FORMAT_POSTMAN,
	BACKEND_FORMAT_TEMPLATE,
	BACKEND_FORMAT_FRAME,
	BACKEND_FORMAT_NUM_MAX
};
extern const char *backend_format_labels[];
typedef enum backend_format backend_format_t;

enum ble_mode {
	BLE_MODE_LEGACY = 0,
	BLE_MODE_EXTENDED,
	BLE_MODE_LONG_RANGE,
	BLE_MODE_NUM_MAX
};
extern const char *ble_mode_labels[];
typedef enum ble_mode ble_mode_t;

enum device_status {
	DEVICE_STATUS_WORKING = 0,
	DEVICE_STATUS_ERROR,
	DEVICE_STATUS_UNSEEN,
	DEVICE_STATUS_NUM_MAX
};
extern const char *device_status_labels[];

enum part {
	PART_NONE = 0,
	PART_SHT3X,
	PART_SHT4X,
	PART_HTU21D,
	PART_HTU31D,
	PART_MCP9808,
	PART_TMP117,
	PART_BMP280,
	PART_BMP388,
	PART_LPS2X3X,
	PART_DPS310,
	PART_MLX90614,
	PART_MCP960X,
	PART_BH1750,
	PART_VEML7700,
	PART_TSL2591,
	PART_SCD4X,
	PART_SEN5X,
	PART_DS18B20,
	PART_TMP1826,
	PART_RUUVITAG,
	PART_MINEW_S1,
	PART_XIAOMI_LYWSDCGQ,
	PART_NUM_MAX
};
extern const char *part_labels[];
typedef enum part part_enum_t;

enum metric {
	METRIC_NONE = 0,
	METRIC_Temperature,
	METRIC_Humidity,
	METRIC_Pressure,
	METRIC_CO2,
	METRIC_PM1,
	METRIC_PM2o5,
	METRIC_PM4,
	METRIC_PM10,
	METRIC_VOC,
	METRIC_NOx,
	METRIC_ProbeTemperature,
	METRIC_InfraredTemperature,
	METRIC_InternalTemperature,
	METRIC_LightIntensity,
	METRIC_UpTime,
	METRIC_FreeHeap,
	METRIC_MinimumFreeHeap,
	METRIC_AccelerationX,
	METRIC_AccelerationY,
	METRIC_AccelerationZ,
	METRIC_BatteryLevel,
	METRIC_TxPower,
	METRIC_Movements,
	METRIC_RSSI,
	METRIC_DCvoltage,
	METRIC_ADCvalue,
	METRIC_ProcessorTemperature,
	METRIC_NUM_MAX
};
extern const char *metric_labels[];
typedef enum metric metric_enum_t;

enum unit {
	UNIT_NONE = 0,
	UNIT_Cel,
	UNIT_RH,
	UNIT_hPa,
	UNIT_ppm,
	UNIT_ug_m3,
	UNIT_lux,
	UNIT_m_s2,
	UNIT_V,
	UNIT_dBm,
	UNIT_ratio,
	UNIT_s,
	UNIT_B,
	UNIT_NUM_MAX
};
extern const char *unit_labels[];
typedef enum unit unit_enum_t;

enum wifi_status {
	WIFI_STATUS_DISCONNECTED = 0,
	WIFI_STATUS_ERROR,
	WIFI_STATUS_CONNECTED,
	WIFI_STATUS_ONLINE,
	WIFI_STATUS_NUM_MAX
};
extern const char *wifi_status_labels[];
typedef enum wifi_status wifi_status_t;

#endif