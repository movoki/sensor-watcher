// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <esp_log.h>

#include "adc.h"
#include "application.h"
#include "board.h"
#include "devices.h"
#include "enums.h"
#include "hmac.h"
#include "measurements.h"
#include "now.h"
#include "pbuf.h"
#include "wifi.h"

bool measurements_full = false;
measurements_index_t measurements_count = 0;
measurement_t measurements[MEASUREMENTS_NUM_MAX] = {{0}};

void measurements_entry_to_frame(measurements_index_t index, measurement_frame_t *frame)
{
    memset(frame, 0, sizeof(measurement_frame_t));

    frame->magic = 0x1023;
    frame->node         = measurements[index].node;
    frame->resource     = measurements[index].resource;
    frame->bus          = measurements[index].bus;
    frame->multiplexer  = measurements[index].multiplexer;
    frame->channel      = measurements[index].channel;
    frame->address      = measurements[index].address;
    frame->part         = measurements[index].part;
    frame->parameter    = measurements[index].parameter;
    frame->unit         = measurements[index].unit;
    frame->metric       = measurements[index].metric;
    frame->time         = measurements[index].time;
    frame->value        = measurements[index].value;
}

bool measurements_build_name(pbuf_t *buf, measurements_index_t measurement, char separator)
{
    switch(measurements[measurement].resource) {
    case RESOURCE_I2C:
    case RESOURCE_ONEWIRE:
    case RESOURCE_BLE:
        return pbuf_printf(buf, "%016llX%c%s%c%i%c%i%c%i%c%016llX%c%s%c%i%c%s",
            measurements[measurement].node,
            separator,
            resource_labels[measurements[measurement].resource],
            separator,
            measurements[measurement].bus,
            separator,
            measurements[measurement].multiplexer,
            separator,
            measurements[measurement].channel,
            separator,
            measurements[measurement].address,
            separator,
            parts[measurements[measurement].part].label,
            separator,
            measurements[measurement].parameter,
            separator,
            metric_labels[measurements[measurement].metric]);
    case RESOURCE_ADC:
        return pbuf_printf(buf, "%016llX%c%s%c%i%c%s",
            measurements[measurement].node,
            separator,
            resource_labels[measurements[measurement].resource],
            separator,
            measurements[measurement].parameter,
            separator,
            metric_labels[measurements[measurement].metric]);
    default:
        return pbuf_printf(buf, "%016llX%c%s%c%s",
            measurements[measurement].node,
            separator,
            resource_labels[measurements[measurement].resource],
            separator,
            metric_labels[measurements[measurement].metric]);
    }
}

void measurements_init()
{
    measurements_full = false;
    measurements_count = 0;
    memset(measurements, 0, sizeof(measurements));	
}

void measurements_measure()
{
    devices_measure_all();
    adc_measure();
    if(application.diagnostics) {
        application_measure();
        board_measure();
    }
}

uint32_t measurements_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    if(method == PM_GET)
        return measurements_pack(writer) ? PM_205_Content : PM_500_Internal_Server_Error;
    else
        return PM_405_Method_Not_Allowed;
}

bool measurements_pack(bp_pack_t *bp)
{
    bool ok = true;
    char name[MEASUREMENTS_NAME_LENGTH];
    pbuf_t buf = { name, sizeof(name), 0 };
    measurements_index_t index = 0;
    measurements_index_t count = measurements_full ? MEASUREMENTS_NUM_MAX : measurements_count;

    ok = ok && bp_create_container(bp, BP_LIST);
    for(int n = 0; n < count && ok; n++) {
        index = measurements_full ? (measurements_count + n) % MEASUREMENTS_NUM_MAX : n;
        buf.length = 0;
        ok = ok && measurements_build_name(&buf, index, '_');
        ok = ok && bp_create_container(bp, BP_LIST);
            ok = ok && bp_put_string(bp, name);
            ok = ok && bp_put_big_integer(bp, measurements[index].time);
            ok = ok && bp_put_string(bp, unit_labels[measurements[index].unit]);
            ok = ok && bp_put_float(bp, measurements[index].value);
        ok = ok && bp_finish_container(bp);
    }
    ok = ok && bp_finish_container(bp);
    return ok;
}

bool measurements_put_signature(bp_pack_t *bp, char *id, char *key)
{
    bool ok = true;
    hmac_sha256_hash_t hash;
    hmac_sha256_key_t binary_key;

    ok = ok && bp_put_big_integer(bp, NOW);
    ok = ok && bp_put_string(bp, id);
    ok = ok && (hmac_hex_decode(binary_key, sizeof(binary_key), key, strlen(key)) == sizeof(binary_key));
    hmac_sha256_sign((uint8_t *) bp->cursor->parent_start, bp_get_offset(bp) * sizeof(bp_type_t), binary_key, hash);
    ok = ok && bp_put_binary(bp, (bp_type_t *) hash, sizeof(hash) / sizeof(bp_type_t));
    return ok;
}

bool measurements_entry_to_postman(measurements_index_t index, char *buffer, size_t *buffer_size, char *id, char *key)
{
    bool ok = true;
    char name[MEASUREMENTS_NAME_LENGTH];
    pbuf_t buf = { name, sizeof(name), 0 };

    bp_pack_t bp;
    bp_set_buffer(&bp, (bp_type_t *) buffer, *buffer_size / sizeof(bp_type_t));

    ok = ok && measurements_build_name(&buf, index, '_');
    ok = ok && bp_put_integer(&bp, PM_205_Content << 24 | 0);
    ok = ok && bp_create_container(&bp, BP_LIST);
        ok = ok && bp_put_string(&bp, "measurements");
    ok = ok && bp_finish_container(&bp);
    ok = ok && bp_create_container(&bp, BP_LIST);
        ok = ok && bp_create_container(&bp, BP_LIST);
            ok = ok && bp_put_string(&bp, name);
            ok = ok && bp_put_big_integer(&bp, measurements[index].time);
            ok = ok && bp_put_string(&bp, unit_labels[measurements[index].unit]);
            ok = ok && bp_put_float(&bp, measurements[index].value);
        ok = ok && bp_finish_container(&bp);
    ok = ok && bp_finish_container(&bp);

    if(id && key && ok)
        ok = ok && measurements_put_signature(&bp, id, key);

    *buffer_size = ok ? bp_get_offset(&bp) * sizeof(bp_type_t) : 0;
    return ok;
}

bool measurements_to_postman(char *buffer, size_t *buffer_size, char *id, char *key)
{
    bp_pack_t bp;
    bool ok = true;

    bp_set_buffer(&bp, (bp_type_t *) buffer, *buffer_size / sizeof(bp_type_t));
    ok = ok && bp_put_integer(&bp, PM_205_Content << 24 | 0);
    ok = ok && bp_create_container(&bp, BP_LIST);
        ok = ok && bp_put_string(&bp, "measurements");
    ok = ok && bp_finish_container(&bp);
    ok = ok && measurements_pack(&bp);
    if(id && key && ok)
        ok = ok && measurements_put_signature(&bp, id, key);

    *buffer_size = ok ? bp_get_offset(&bp) * sizeof(bp_type_t) : 0;
    return ok;
}

bool measurements_entry_to_senml_row(measurements_index_t index, pbuf_t *buf)
{
    bool ok = true;
    ok = ok && pbuf_printf(buf,"{\"n\":\"urn:dev:mac:");
    ok = ok && measurements_build_name(buf, index, '_');
    ok = ok && pbuf_printf(buf,"\",\"u\":\"%s\",\"v\":%f,\"t\":%lli}", unit_labels[measurements[index].unit], measurements[index].value, (int64_t) measurements[index].time);
    return ok;
}

bool measurements_to_senml(char *buffer, size_t *buffer_size)
{
    bool ok = true;
    pbuf_t buf = { buffer, *buffer_size, 0 };
    measurements_index_t index = 0;
    measurements_index_t count = measurements_full ? MEASUREMENTS_NUM_MAX : measurements_count;

    ok = ok && pbuf_putc(&buf, '[');
    for(int n = 0; n != count && ok; n++) {
        index = measurements_full ? (measurements_count + n) % MEASUREMENTS_NUM_MAX : n;
        ok = ok && measurements_entry_to_senml_row(index, &buf);
        if(n != count - 1)
            ok = ok && pbuf_putc(&buf, ',');
    }
    ok = ok && pbuf_putc(&buf, ']');

    *buffer_size = ok ? buf.length : 0;
    return ok;
}

bool measurements_entry_to_template_row(measurements_index_t index, pbuf_t *buf, char *template_row, char *template_name_separator)
{
    bool ok = true;
    int template_row_length = strlen(template_row);
    for(int j = 0; j < template_row_length && ok; j++) {
        if(template_row[j] == '@' && j != template_row_length - 1) {
            switch(template_row[j + 1]) {
                case '@': ok = ok && pbuf_putc(buf, '@'); break;
                case 'I': ok = ok && pbuf_printf(buf, "%016llX", wifi.mac); break;
                case 'N': ok = ok && measurements_build_name(buf, index, template_name_separator[0]); break;
                case 'R': ok = ok && pbuf_printf(buf, "%s", resource_labels[measurements[index].resource]); break;
                case 'b': ok = ok && pbuf_printf(buf, "%u", measurements[index].bus); break;
                case 'm': ok = ok && pbuf_printf(buf, "%u", measurements[index].multiplexer); break;
                case 'c': ok = ok && pbuf_printf(buf, "%u", measurements[index].channel); break;
                case 'A': ok = ok && pbuf_printf(buf, "%016llX", measurements[index].address); break;
                case 'P': ok = ok && pbuf_printf(buf, "%s", parts[measurements[index].part].label); break;
                case 'p': ok = ok && pbuf_printf(buf, "%u", measurements[index].parameter); break;
                case 'M': ok = ok && pbuf_printf(buf, "%s", metric_labels[measurements[index].metric]); break;
                case 'U': ok = ok && pbuf_printf(buf, "%s", unit_labels[measurements[index].unit]); break;
                case 'v': ok = ok && pbuf_printf(buf, "%f", measurements[index].value); break;
                case 't': ok = ok && pbuf_printf(buf, "%lli", (int64_t) measurements[index].time); break;
                default:  ok = ok && pbuf_printf(buf, "%%_");
            }
            j += 1;
        }
        else
            ok = ok && pbuf_putc(buf, template_row[j]);
    }
    return ok;
}

bool measurements_to_template(char *buffer, size_t *buffer_size, char *template_header, char *template_row, char *template_row_separator, char *template_name_separator, char *template_footer)
{
    bool ok = true;
    pbuf_t buf = { buffer, *buffer_size, 0 };
    measurements_index_t index = 0;
    measurements_index_t count = measurements_full ? MEASUREMENTS_NUM_MAX : measurements_count;

    ok = ok && pbuf_printf(&buf, "%s", template_header);
    for(int n = 0; n != count && ok; n++) {
        index = measurements_full ? (measurements_count + n) % MEASUREMENTS_NUM_MAX : n;
        ok = ok && measurements_entry_to_template_row(index, &buf, template_row, template_name_separator);
        if(n != count - 1)
            ok = ok && pbuf_printf(&buf, "%s", template_row_separator);
    }
    ok = ok && pbuf_printf(&buf, "%s", template_footer);

    *buffer_size = ok ? buf.length : 0;
    return ok;
}

bool measurements_append(measurement_node_t node,          resource_t resource,          device_bus_t bus,
                         device_multiplexer_t multiplexer, device_channel_t channel,     device_address_t address,
                         device_part_t part,               device_parameter_t parameter, measurement_metric_t metric,
                         time_t time,                      measurement_unit_t unit,      float value) // 😱
{
    if((application.queue || !measurements_full) && (!application.queue || time > 1680000000) && metric < METRIC_NUM_MAX && unit < UNIT_NUM_MAX) {
        measurements[measurements_count].node = node;
        measurements[measurements_count].resource = resource;
        measurements[measurements_count].bus = bus;
        measurements[measurements_count].multiplexer = multiplexer;
        measurements[measurements_count].channel = channel;
        measurements[measurements_count].address = address;
        measurements[measurements_count].part = part;
        measurements[measurements_count].parameter = parameter;
        measurements[measurements_count].metric = metric;
        measurements[measurements_count].time = time > 1680000000 ? time : 0;
        measurements[measurements_count].unit = unit;
        measurements[measurements_count].value = value;

        measurements_full = measurements_full ? true : measurements_count == MEASUREMENTS_NUM_MAX - 1;
        measurements_count = (measurements_count + 1) % MEASUREMENTS_NUM_MAX;
        return true;
    }
    else
        return false;
}

bool measurements_append_from_device(devices_index_t device, device_parameter_t parameter, measurement_metric_t metric, time_t time, measurement_unit_t unit, float value)
{
    if(device < DEVICES_NUM_MAX && parameter < DEVICES_PARAMETERS_NUM_MAX && metric < METRIC_NUM_MAX && unit < UNIT_NUM_MAX)
        return measurements_append(wifi.mac, devices[device].resource, devices[device].bus, devices[device].multiplexer,
                                   devices[device].channel, devices[device].address, devices[device].part, parameter,
                                   metric, time, unit, value + devices[device].offsets[parameter]);
    else
        return false;
}

bool measurements_append_from_frame(measurement_frame_t *frame)
{
    return measurements_append(frame->node,    frame->resource, frame->bus,  frame->multiplexer,
                               frame->channel, frame->address,  frame->part, frame->parameter,
                               frame->metric,  frame->time,     frame->unit, frame->value);
}
