// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <nvs_flash.h>
#include <driver/gpio.h>

#include "adc.h"
#include "application.h"
#include "board.h"
#include "enums.h"
#include "measurements.h"
#include "now.h"
#include "schema.h"
#include "wifi.h"

adc_t adc;

void adc_init()
{
    adc_read_from_nvs();
}

bool adc_read_from_nvs()
{
    esp_err_t err;
    nvs_handle_t handle;
    int32_t multiplier;

    err = nvs_open("adc", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        if(nvs_get_i32(handle, "channels", &(adc.channels)) != ESP_OK)
            adc.channels = 0;
        if(nvs_get_u8(handle, "power_pin", &(adc.power_pin)) != ESP_OK)
            adc.power_pin = 0xFF;
        if(nvs_get_i32(handle, "multiplier", &(multiplier)) != ESP_OK)
            adc.multiplier = 1;
        else
            adc.multiplier = multiplier / 1000.0;
        nvs_close(handle);
        ESP_LOGI(__func__, "done");
        return true;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

bool adc_write_to_nvs()
{
    esp_err_t err;
    bool ok = true;
    nvs_handle_t handle;

    err = nvs_open("adc", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        ok = ok && !nvs_set_i32(handle, "channels", adc.channels);
        ok = ok && !nvs_set_u8(handle, "power_pin", adc.power_pin);
        ok = ok && !nvs_set_i32(handle, "multiplier", (int32_t) (adc.multiplier * 1000));
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

static bool write_resource_schema(bp_pack_t *writer)
{
    int gpio;
    bool ok = true;
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_put_integer(writer, SCHEMA_MAP);
        ok = ok && bp_create_container(writer, BP_MAP);

            ok = ok && bp_put_string(writer, "channels");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_LIST | SCHEMA_UNIQUE);
                ok = ok && bp_create_container(writer, BP_LIST);
                    ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_VALUES);
                    ok = ok && bp_create_container(writer, BP_LIST);
                        for(int i = 0; i <= ADC_CHANNEL_9; i++) {
                            adc_oneshot_channel_to_io(ADC_UNIT_1, i, &gpio);
                            ok = ok && bp_put_integer(writer, gpio);
                        }
                    ok = ok && bp_finish_container(writer);
                ok = ok && bp_finish_container(writer);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "power_pin");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_NULL | SCHEMA_MINIMUM | SCHEMA_MAXIMUM);
                ok = ok && bp_put_integer(writer, 0);
                ok = ok && bp_put_integer(writer, GPIO_NUM_MAX - 1);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "multiplier");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_FLOAT);
            ok = ok && bp_finish_container(writer);

        ok = ok && bp_finish_container(writer);
    ok = ok && bp_finish_container(writer);
    return ok;
}

bool adc_schema_handler(char *resource_name, bp_pack_t *writer)
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

uint32_t adc_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    bool ok = true;
    uint32_t response = 0;

    if(method == PM_GET) {
        int gpio;
        ok = ok && bp_create_container(writer, BP_MAP);
            ok = ok && bp_put_string(writer, "channels");
            ok = ok && bp_create_container(writer, BP_LIST);
                for(int i = 0; i < sizeof(adc.channels) * 8 && ok; i++) {
                    if(adc.channels & (1 << i)) {
                        ok = ok && adc_oneshot_channel_to_io(ADC_UNIT_1, i, &gpio) == ESP_OK;
                        ok = ok && bp_put_integer(writer, gpio);
                    }
                }
            ok = ok && bp_finish_container(writer);
            ok = ok && bp_put_string(writer, "power_pin");
            ok = ok && (adc.power_pin == 0xFF ? bp_put_none(writer) : bp_put_integer(writer, adc.power_pin));
            ok = ok && bp_put_string(writer, "multiplier");
            ok = ok && bp_put_float(writer, adc.multiplier);
        ok = ok && bp_finish_container(writer);
        return ok ? PM_205_Content : PM_500_Internal_Server_Error;
    }
    else if(method == PM_PUT) {
        if(!bp_close(reader) || !bp_next(reader) || !bp_is_map(reader) || !bp_open(reader))
            response = PM_400_Bad_Request;
        else {
            while(ok && bp_next(reader)) {
                if(bp_match(reader, "channels")) {
                    int32_t channels = 0;
                    adc_unit_t unit_id;
                    adc_channel_t channel = 0;
                    if(bp_is_list(reader) && bp_open(reader)) {
                        while(ok && bp_next(reader)) {
                            ok = ok && bp_is_integer(reader);
                            ok = ok && adc_oneshot_io_to_channel(bp_get_integer(reader), &unit_id, &channel) == ESP_OK;
                            ok = ok && unit_id == ADC_UNIT_1;
                            channels |= 1 << channel;
                        }
                        bp_close(reader);
                        if(ok)
                            adc.channels = channels;
                    }
                    else
                        ok = false;
                }
                else if(bp_match(reader, "power_pin"))
                    adc.power_pin = bp_is_none(reader) ? 0xFF : bp_get_integer(reader);
                else if(bp_match(reader, "multiplier"))
                    adc.multiplier = bp_get_float(reader);
                else
                    bp_next(reader);
            }
            bp_close(reader);
            if(ok)
                response = adc_write_to_nvs() ? PM_204_Changed : PM_500_Internal_Server_Error;
            else
                response = PM_400_Bad_Request;
        }
        return response;
    }
    else
        return PM_405_Method_Not_Allowed;
}

bool adc_measure()
{
    bool ok = true;
    if(adc.channels) {
        int gpio;
        int adc_raw;
        int voltage;

        if(adc.power_pin != 0xFF) {
            gpio_config_t io_conf = {};
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = 1ULL << adc.power_pin;
            io_conf.pull_down_en = 0;
            io_conf.pull_up_en = 0;
            gpio_config(&io_conf);
            gpio_set_level(adc.power_pin, true);
        }

        adc_cali_handle_t cali_handle = NULL;
        adc_oneshot_unit_handle_t handle = NULL;
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };

        ok = ok && adc_oneshot_new_unit(&init_config, &handle) == ESP_OK;

        // configure channels
        adc_oneshot_chan_cfg_t config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        for(int channel = 0; channel != ADC_CHANNELS_NUM_MAX && ok; channel++)
            if(adc.channels & (1 << channel))
                ok = ok && adc_oneshot_config_channel(handle, channel, &config) == ESP_OK;

        // calibrate
        #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
            adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = ADC_UNIT_1,
                .atten = ADC_ATTEN_DB_12,
                .bitwidth = ADC_BITWIDTH_12,
            };
            ok = ok && adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle) == ESP_OK;
        #elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
            adc_cali_line_fitting_config_t cali_config = {
                .unit_id = ADC_UNIT_1,
                .atten = ADC_ATTEN_DB_12,
                .bitwidth = ADC_BITWIDTH_12,
            };
            ok = ok && adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle) == ESP_OK;
        #endif

        // measure channels
        for(int channel = 0; channel != ADC_CHANNELS_NUM_MAX && ok; channel++) {
            if(adc.channels & (1 << channel)) {
                ok = ok && adc_oneshot_channel_to_io(ADC_UNIT_1, channel, &gpio) == ESP_OK;
                ok = ok && adc_oneshot_read(handle, channel, &adc_raw) == ESP_OK;
                if(cali_handle != NULL) {
                    ok = ok && adc_cali_raw_to_voltage(cali_handle, adc_raw, &voltage);
                    measurements_append(board.id, RESOURCE_ADC, 0, 0, 0, 0, 0, gpio, METRIC_DCvoltage, NOW, UNIT_V, voltage * adc.multiplier / 1000.0);
                }
                else
                    measurements_append(board.id, RESOURCE_ADC, 0, 0, 0, 0, 0, gpio, METRIC_ADCvalue, NOW, UNIT_NONE, adc_raw);
            }
        }

        if(cali_handle != NULL) {
            #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
                adc_cali_delete_scheme_curve_fitting(cali_handle);
            #elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
                adc_cali_delete_scheme_line_fitting(cali_handle);
            #endif
        }
        if(handle != NULL)
            adc_oneshot_del_unit(handle);

        if(adc.power_pin != 0xFF)
            gpio_set_level(adc.power_pin, false);
    }
    return ok;
}
