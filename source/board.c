// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <esp_mac.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <nvs_flash.h>
#include <driver/i2c.h>
#include <driver/temperature_sensor.h>

#include "application.h"
#include "postman.h"
#include "board.h"
#include "i2c.h"
#include "devices.h"
#include "enums.h"
#include "measurements.h"
#include "now.h"
#include "schema.h"
#include "wifi.h"

board_t board;

uint8_t led_gpio = 0xFF;
uint8_t bus_power_gpio = 0xFF;
temperature_sensor_handle_t cpu_temp_sensor = NULL;


void board_init()
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    board.processor = chip.model;

    esp_base_mac_addr_get((uint8_t *) &board.id);
    board.id = __builtin_bswap64(board.id);
    if((board.id & 0xFFFF) == 0)
        board.id = (board.id & 0xFFFFFF0000000000) | 0x000000FFFF000000 | ((board.id & 0x000000FFFFFF0000) >> 16);

    board_read_from_nvs();
    esp_log_level_set("*", board.log_level);
    board_configure();
}

void board_configure()
{
    gpio_config_t io_conf;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    #ifdef CONFIG_IDF_TARGET_ESP32S3
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    temperature_sensor_install(&temp_sensor_config, &cpu_temp_sensor);
    temperature_sensor_enable(cpu_temp_sensor);
    #endif

    switch(board.model) {
    case BOARD_MODEL_M5STACK_M5STICKC:
    case BOARD_MODEL_M5STACK_M5STICKC_PLUS:
        led_gpio = 10;
        io_conf.pin_bit_mask = (1ULL << led_gpio);
        gpio_config(&io_conf);
        break;
    case BOARD_MODEL_ADAFRUIT_ESP32_FEATHER_V2:
        bus_power_gpio = 2;
        io_conf.pin_bit_mask = (1ULL << bus_power_gpio);
        gpio_config(&io_conf);
        break;
    }

    // board_set_led(0x0000FF);
}

bool board_read_from_nvs()
{
    esp_err_t err;
    nvs_handle_t handle;

    err = nvs_open("board", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        if(nvs_get_u32(handle, "model", &(board.model)) != ESP_OK)
            board.model = 0;
        if(nvs_get_u32(handle, "log_level", &(board.log_level)) != ESP_OK)
            board.log_level = ESP_LOG_INFO;
        nvs_get_u8(handle, "diagnostics", (uint8_t *) &(board.diagnostics));
        nvs_close(handle);
        ESP_LOGI(__func__, "done");
        return true;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

bool board_write_to_nvs()
{
    esp_err_t err;
    bool ok = true;
    nvs_handle_t handle;

    err = nvs_open("board", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        ok = ok && !nvs_set_u32(handle, "model", board.model);
        ok = ok && !nvs_set_u32(handle, "log_level", board.log_level);
        ok = ok && !nvs_set_u8(handle, "diagnostics", board.diagnostics);
        ok = ok && !nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(__func__, "%s", ok ? "done" : "failed");
        return ok;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

char* board_get_processor_label()
{
    switch(board.processor) {
    case CHIP_ESP32:   return "ESP32";
    case CHIP_ESP32S3: return "ESP32-S3";
    case CHIP_ESP32C3: return "ESP32-C3";
    case CHIP_ESP32C6: return "ESP32-C6";
    default:           return "";
    }
}

uint32_t board_get_flash_size()
{
    uint32_t flash_id;
    esp_flash_read_id(NULL, &flash_id);

    uint8_t n = (flash_id & 0xFF) - 0x12;
    return n < 7 ? 0x100 << n : 0;
}

static bool write_resource_schema(bp_pack_t *writer)
{
    bool ok = true;
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_put_integer(writer, SCHEMA_MAP);
        ok = ok && bp_create_container(writer, BP_MAP);

            ok = ok && bp_put_string(writer, "id");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_READ_ONLY | SCHEMA_MINIMUM_BYTES | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, 16 + 1);
                ok = ok && bp_put_integer(writer, 16 + 1);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "processor");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_READ_ONLY | SCHEMA_VALUES);
                ok = ok && bp_create_container(writer, BP_LIST);
                    ok = ok && bp_put_string(writer, "ESP32");
                    ok = ok && bp_put_string(writer, "ESP32-S3");
                    ok = ok && bp_put_string(writer, "ESP32-C3");
                    ok = ok && bp_put_string(writer, "ESP32-C6");
                ok = ok && bp_finish_container(writer);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "flash_size");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_READ_ONLY);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "model");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_VALUES);
                ok = ok && bp_create_container(writer, BP_LIST);
                for(int i = 0; i < BOARD_MODEL_NUM_MAX; i++)
                    ok = ok && bp_put_string(writer, board_model_labels[i]);
                ok = ok && bp_finish_container(writer);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "log_level");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_MINIMUM | SCHEMA_MAXIMUM);
                ok = ok && bp_put_integer(writer, 0);
                ok = ok && bp_put_integer(writer, CONFIG_LOG_MAXIMUM_LEVEL);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "diagnostics");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_BOOLEAN);
            ok = ok && bp_finish_container(writer);

        ok = ok && bp_finish_container(writer);
    ok = ok && bp_finish_container(writer);
    return ok;
}

bool board_schema_handler(char *resource_name, bp_pack_t *writer)
{
    bool ok = true;

    // GET / PUT
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_create_container(writer, BP_LIST);                                // Path
            ok = ok && bp_put_string(writer, resource_name);
        ok = ok && bp_finish_container(writer);
        ok = ok && bp_put_integer(writer, SCHEMA_GET_RESPONSE | SCHEMA_PUT_REQUEST);    // Methods
        ok = ok && write_resource_schema(writer);                                       // Schema
    ok = ok && bp_finish_container(writer);

    return ok;
}


uint32_t board_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    bool ok = true;
    uint32_t response = 0;

    if(method == PM_GET) {
        char id_string[17];
        snprintf(id_string, sizeof(id_string), "%016llX", board.id);
        ok = ok && bp_create_container(writer, BP_MAP);
        ok = ok && bp_put_string(writer, "id");
        ok = ok && bp_put_string(writer, id_string);
        ok = ok && bp_put_string(writer, "processor");
        ok = ok && bp_put_string(writer, board_get_processor_label());
        ok = ok && bp_put_string(writer, "flash_size");
        ok = ok && bp_put_integer(writer, board_get_flash_size());
        ok = ok && bp_put_string(writer, "model");
        ok = ok && bp_put_string(writer, board_model_labels[board.model < BOARD_MODEL_NUM_MAX ? board.model : 0]);
        ok = ok && bp_put_string(writer, "log_level");
        ok = ok && bp_put_integer(writer, board.log_level);
        ok = ok && bp_put_string(writer, "diagnostics");
        ok = ok && bp_put_boolean(writer, board.diagnostics);
        ok = ok && bp_finish_container(writer);
        return ok ? PM_205_Content : PM_500_Internal_Server_Error;
    }
    else if(method == PM_PUT) {
        if(!bp_close(reader) || !bp_next(reader) || !bp_is_map(reader) || !bp_open(reader))
            response = PM_400_Bad_Request;
        else {
            while(ok && bp_next(reader)) {
                if(bp_match(reader, "model")) {
                    devices_buses_stop();
                    int m;
                    for(m = 0; m < BOARD_MODEL_NUM_MAX; m++)
                        if(bp_equals(reader, board_model_labels[m]))
                            break;
                    if(m < BOARD_MODEL_NUM_MAX)
                        board.model = m;
                    else
                        ok = false;
                    board_configure();
                    devices_buses_init();
                    devices_init();
                    measurements_init();
                    measurements_measure();
                }
                else if(bp_match(reader, "log_level")) {
                    board.log_level = bp_get_integer(reader);
                    esp_log_level_set("*", board.log_level);
                }
                else if(bp_match(reader, "diagnostics"))
                    board.diagnostics = bp_get_boolean(reader);
                else bp_next(reader);
            }
            bp_close(reader);
            ok = ok && board_write_to_nvs();
            response = ok ? PM_204_Changed : PM_500_Internal_Server_Error;
        }
        return response;
    }
    else
        return PM_405_Method_Not_Allowed;
}

void board_measure()
{
    #if defined (CONFIG_IDF_TARGET_ESP32S2) || defined (CONFIG_IDF_TARGET_ESP32S3) || defined (CONFIG_IDF_TARGET_ESP32C2) || defined (CONFIG_IDF_TARGET_ESP32C3) || defined (CONFIG_IDF_TARGET_ESP32C6) || defined (CONFIG_IDF_TARGET_ESP32H2)
        float cpu_temp;
        if(board.diagnostics && cpu_temp_sensor && temperature_sensor_get_celsius(cpu_temp_sensor, &cpu_temp) == ESP_OK)
            measurements_append(board.id, RESOURCE_BOARD, 0, 0, 0, 0, 0, 0, METRIC_ProcessorTemperature, NOW, UNIT_Cel, cpu_temp);
    #endif
}

void board_set_led(uint32_t color) 
{
    switch(board.model) {
    case BOARD_MODEL_M5STACK_M5STICKC:
    case BOARD_MODEL_M5STACK_M5STICKC_PLUS:
        if(led_gpio != 0xFF)
            gpio_set_level(led_gpio, color ? 0 : 1);
        break;
    }
}

void board_set_I2C_power(bool state)
{
    switch(board.model) {
    case BOARD_MODEL_M5STACK_M5STICKC:
    case BOARD_MODEL_M5STACK_M5STICKC_PLUS:
    case BOARD_MODEL_M5STACK_CORE2:
    case BOARD_MODEL_M5STACK_CORE2_AWS:
    case BOARD_MODEL_M5STACK_TOUGH:
    case BOARD_MODEL_M5STACK_M5STATION_BAT:
    case BOARD_MODEL_M5STACK_M5STATION_485:
        if(i2c_buses_count && i2c_buses[0].port == 0) {
            uint8_t cmd[] = { 0x10, state ? 0x04 : 0x00 };
            i2c_master_write_to_device(0x00, 0x34, cmd, sizeof(cmd), 1000 / portTICK_PERIOD_MS);
        }
        break;
    case BOARD_MODEL_ADAFRUIT_ESP32_FEATHER_V2:
        if(bus_power_gpio != 0xFF)
            gpio_set_level(bus_power_gpio, state ? 1 : 0);
        break;        
    }
}    
