// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <ctype.h>

#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <nvs_flash.h>
#include <driver/uart.h>

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define USE_USB_CDC true
#endif

#ifdef USE_USB_CDC
#include <tinyusb.h>
#include <tusb_cdc_acm.h>
#endif

#include <esp_tls.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <mqtt_client.h>

#include "yuarel.h"

#include "framer.h"
#include "postman.h"

#include "adc.h"
#include "application.h"
#include "ble.h"
#include "board.h"
#include "backends.h"
#include "devices.h"
#include "enums.h"
#include "i2c.h"
#include "logs.h"
#include "measurements.h"
#include "onewire.h"
#include "wifi.h"
#include "now.h"


#define WIFI_CONNECT_DELAY  10 // seconds
#define SNTP_SYNC_DELAY     30 // seconds

#define POSTMAN_PACKET_LENGTH_MAX   (9 * 1024)          // 9KB, to fit a full packed backend_t plus headers
#define UART_BUFFER_SIZE    POSTMAN_PACKET_LENGTH_MAX   // To contain a full 'put backends/x' and avoid byte losses when busy HTTPing
#define UART_NUMBER         UART_NUM_0

framer_t framer;
postman_t postman;

size_t http_response_length = 0;
alignas(4) uint8_t http_response_buffer[POSTMAN_PACKET_LENGTH_MAX];
uint32_t postman_buffer[POSTMAN_PACKET_LENGTH_MAX / 4];
char backend_buffer[9 * 1024];      // 9 KB, enough for 64 measurements coded as Postman


bool sntp_started = false;
extern bool backends_started;
RTC_DATA_ATTR bool slept_once = false;

void nvs_init()
{
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        err = err ? err : nvs_flash_init();
    }
    ESP_LOGI(__func__, "%s", err ? "failed" : "done");
}

void serial_init()
{
    esp_err_t err = ESP_OK;
#ifdef USE_USB_CDC
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };

    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = UART_BUFFER_SIZE,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };

    err = err ? err : tinyusb_driver_install(&tusb_cfg);
    err = err ? err : tusb_cdc_acm_init(&acm_cfg);
#else
    uart_config_t config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };
    err = err ? err : uart_param_config(UART_NUMBER, &config);
    err = err ? err : uart_driver_install(UART_NUMBER, UART_BUFFER_SIZE, UART_BUFFER_SIZE, 0, NULL, 0);
#endif
    ESP_LOGI(__func__, "%s", err ? "failed" : "done");
}

int serial_write_bytes(uint8_t *bytes, size_t length)
{
#ifdef USE_USB_CDC
    return tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, bytes, length);
#else
    return uart_write_bytes(0, bytes, length);
#endif
}

int serial_read_bytes(uint8_t *bytes, size_t length)
{
#ifdef USE_USB_CDC
    return tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0, bytes, length, &length) == ESP_OK ? length : 0;
#else
    return uart_read_bytes(0, bytes, length, 0);
#endif
}

void serial_flush()
{
#ifdef USE_USB_CDC
    tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
#endif
}

void serial_send_receive()
{
    uint8_t byte;
    while(framer.state == FRAMER_SENDING) {
        byte = framer_get_byte_to_send(&framer);
        serial_write_bytes(&byte, 1);
    }
    serial_flush();
    while(framer.state == FRAMER_RECEIVING && serial_read_bytes(&byte, 1)) {
        if(framer_put_received_byte(&framer, byte) && framer.length) {
            framer.length = postman_handle_pack(&postman, postman_buffer, framer.length / 4, sizeof(postman_buffer) / 4, 0, NULL, NULL) * 4;
            framer_set_state(&framer, FRAMER_SENDING);
            break;
        }
    }
}

#define MIN(a,b) (((a)<(b))?(a):(b))

esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    switch(event->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
             http_response_length = 0;
             http_response_buffer[0] = 0;
             break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(event->client)) {
                size_t copy_len = MIN(event->data_len, (sizeof(http_response_buffer) - 1 - http_response_length));
                if(copy_len)
                    memcpy(http_response_buffer + http_response_length, event->data, copy_len);
                http_response_length += copy_len;
                http_response_buffer[http_response_length] = 0;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

size_t encode_measurements(uint8_t backend_index)
{
    bool ok = true;
    size_t backend_buffer_length = sizeof(backend_buffer);

    switch(backends[backend_index].format) {
        case BACKEND_FORMAT_SENML:
            ok = ok && measurements_to_senml(backend_buffer, &backend_buffer_length);
            break;
        case BACKEND_FORMAT_POSTMAN:
            ok = ok && measurements_to_postman(backend_buffer, &backend_buffer_length,
                backends[backend_index].auth == BACKEND_AUTH_POSTMAN ? backends[backend_index].user : NULL,
                backends[backend_index].auth == BACKEND_AUTH_POSTMAN ? backends[backend_index].key : NULL);
            break;
        case BACKEND_FORMAT_TEMPLATE:
            ok = ok && measurements_to_template(backend_buffer, &backend_buffer_length,
                backends[backend_index].template_header, backends[backend_index].template_row,
                backends[backend_index].template_row_separator, backends[backend_index].template_name_separator,
                backends[backend_index].template_footer);
            break;
        default:
            ok = false;
    }

    ESP_LOGI(__func__, "backend buffer length / size: %u / %u", backend_buffer_length, sizeof(backend_buffer));

    if(!ok && !backend_buffer_length)
        ESP_LOGE(__func__, "backend buffer overflow!");

    if(!ok) {
        backends[backend_index].status = BACKEND_STATUS_ERROR;
        backends[backend_index].error = 0x201; // Serialization failed
        backends[backend_index].message[0] = 0;
        backend_buffer_length = 0;
    }
    return backend_buffer_length;
}

void app_main(void)
{
    esp_err_t err;
    int64_t now;
    bool ready_to_sleep = false;
    bool measurements_updated = false;

    esp_event_loop_create_default();

    logs_init();        // order of inits is important!
    serial_init();
    nvs_init();         // 100 ms
    wifi_init();        // 160 ms
    board_init();
    application_init();
    adc_init();
    i2c_init();
    onewire_init();
    ble_init();
    backends_init(); // 47 ms
    measurements_init();
    ESP_LOGI(__func__, "measurements_init ended @ %lli", esp_timer_get_time());

    if(!slept_once) {
        devices_init(); // 64 ms
        vTaskDelay (50 / portTICK_PERIOD_MS);  // Wait for I2C devices to stabilize after configuration
        ESP_LOGI(__func__, "devices_init ended @ %lli", esp_timer_get_time());
    }

    framer_set_buffer(&framer, (uint8_t *)postman_buffer, sizeof(postman_buffer));
    postman_init(&postman);
    postman_register_resource(&postman, "adc", &adc_resource_handler);
    postman_register_resource(&postman, "application", &application_resource_handler);
    postman_register_resource(&postman, "ble", &ble_resource_handler);
    postman_register_resource(&postman, "board", &board_resource_handler);
    postman_register_resource(&postman, "backends", &backends_resource_handler);
    postman_register_resource(&postman, "devices", &devices_resource_handler);
    postman_register_resource(&postman, "enums", &enums_resource_handler);
    postman_register_resource(&postman, "i2c", &i2c_resource_handler);
    postman_register_resource(&postman, "logs", &logs_resource_handler);
    postman_register_resource(&postman, "measurements", &measurements_resource_handler);
    postman_register_resource(&postman, "onewire", &onewire_resource_handler);
    postman_register_resource(&postman, "wifi", &wifi_resource_handler);

    application.next_measurement_time += (ble.enabled ? ble.scan_duration * 1000000 : 0);

    ESP_LOGI(__func__, "inits ended @ %lli", esp_timer_get_time());
    ESP_LOGI(__func__, "application.next_measurement_time: %lli", application.next_measurement_time);

    ESP_LOGI(__func__, "sizeof devices: %u", sizeof(device_t) * DEVICES_NUM_MAX);
    ESP_LOGI(__func__, "sizeof measurements: %u", sizeof(measurement_t) * MEASUREMENTS_NUM_MAX);
    ESP_LOGI(__func__, "sizeof i2c_buses: %u", sizeof(i2c_bus_t) * I2C_BUSES_NUM_MAX);
    ESP_LOGI(__func__, "sizeof onewire_buses: %u", sizeof(onewire_bus_t) * ONEWIRE_BUSES_NUM_MAX);

    while(true) {
        serial_send_receive();

        if(!sntp_started && wifi.status == WIFI_STATUS_ONLINE && !application.sleep) {
            sntp_started = true;
            esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
            esp_sntp_setservername(0, "pool.ntp.org");
            esp_sntp_init();
        }

        if(wifi.disconnected) {
            backends_stop();
            backends_clear_status();
            wifi.disconnected = false;
            ESP_LOGI(__func__, "wifi disconnection detected");
        }

        if(wifi.reconnected) {
            backends_start();
            wifi.reconnected = false;
            ESP_LOGI(__func__, "wifi connection detected");
        }

        if(ble.enabled && !ble_is_scanning() && esp_timer_get_time() + ble.scan_duration * 1000000LL >= application.next_measurement_time) {
            ble_start_scan();
            ESP_LOGI(__func__, "starting ble scan @ %lli", esp_timer_get_time());
        }

        now = esp_timer_get_time();
        if(now >= application.next_measurement_time || backends_modified) {
            backends_modified = false;
            ESP_LOGI(__func__, "starting measurements @ %lli", now);
            application.last_measurement_time = now;
            application.next_measurement_time += application.sampling_period * 1000000L;
            ESP_LOGI(__func__, "last_measurement_time %lli next_measurement_time %lli", (long long int)application.last_measurement_time, (long long int)application.next_measurement_time);
            if(!application.queue)
                measurements_init();
            measurements_measure();
            if(ble.enabled) {
                ble_stop_scan();
                ESP_LOGI(__func__, "ble_measurements_count: %lu", ble_measurements_count);
                ble_merge_measurements();
            }
            if(!application.queue && measurements_full)
                ESP_LOGE(__func__, "measurements buffer overflow!");
            measurements_updated = true;
            ESP_LOGI(__func__, "finished measurements @ %lli", esp_timer_get_time());
        }

        if(wifi.status == WIFI_STATUS_ONLINE && measurements_updated && (measurements_count || measurements_full)) {
            if(application.diagnostics)
                wifi_measure();

            for(uint8_t i = 0; i != BACKENDS_NUM_MAX; i++) {
                if(backends[i].uri[0] == 0)
                    continue;

                ESP_LOGI(__func__, "started sending measurements @ %lli", esp_timer_get_time());
                switch(backends[i].uri[0]) {
                case 'h': {     // http / https
                    esp_http_client_config_t config_post = {
                        .url = backends[i].uri,
                        .method = HTTP_METHOD_POST,
                        .cert_pem = backends[i].server_cert[0] ? backends[i].server_cert : NULL,
                        .crt_bundle_attach = backends[i].server_cert[0] ? NULL : esp_crt_bundle_attach,
                        .is_async = false,
                        .timeout_ms = 7000,
                        .event_handler = http_event_handler,
                    };

                    esp_http_client_handle_t client = esp_http_client_init(&config_post);
                    if(!client) {
                        backends[i].status = BACKEND_STATUS_ERROR;
                        backends[i].error = ESP_ERR_INVALID_ARG;
                        backends[i].message[0] = 0;
                        continue;
                    }

                    switch(backends[i].auth) {
                        case BACKEND_AUTH_BASIC:
                            esp_http_client_set_authtype(client, HTTP_AUTH_TYPE_BASIC);
                            esp_http_client_set_username(client, backends[i].user);
                            esp_http_client_set_password(client, backends[i].key);
                            break;
                        case BACKEND_AUTH_DIGEST:
                            esp_http_client_set_authtype(client, HTTP_AUTH_TYPE_DIGEST);
                            esp_http_client_set_username(client, backends[i].user);
                            esp_http_client_set_password(client, backends[i].key);
                            break;
                        case BACKEND_AUTH_BEARER:
                            snprintf(backend_buffer, sizeof(backend_buffer), "Bearer %s", backends[i].key);
                            esp_http_client_set_header(client, "Authorization", backend_buffer);
                            break;
                        case BACKEND_AUTH_TOKEN:
                            snprintf(backend_buffer, sizeof(backend_buffer), "Token %s", backends[i].key);
                            esp_http_client_set_header(client, "Authorization", backend_buffer);
                            break;
                        case BACKEND_AUTH_HEADER:
                            esp_http_client_set_header(client, backends[i].user, backends[i].key);
                            break;
                    }

                    if(backends[i].content_type[0])
                        esp_http_client_set_header(client, "Content-Type", backends[i].content_type);
                    else {
                        switch(backends[i].format) {
                            case BACKEND_FORMAT_SENML:
                                esp_http_client_set_header(client, "Content-Type", "application/json"); break;
                            case BACKEND_FORMAT_POSTMAN:
                                esp_http_client_set_header(client, "Content-Type", "application/vnd.postman"); break;
                            case BACKEND_FORMAT_TEMPLATE:
                                esp_http_client_set_header(client, "Content-Type", "text/plain; charset=utf-8"); break;
                        }
                    }

                    size_t backend_buffer_length = encode_measurements(i);
                    if(!backend_buffer_length)
                        continue;
                    esp_http_client_set_post_field(client, backend_buffer, backend_buffer_length);

                    err = esp_http_client_perform(client);
                    if(err == ESP_OK) {
                        int status = esp_http_client_get_status_code(client);
                        backends[i].status = status < 300 ? BACKEND_STATUS_ONLINE : BACKEND_STATUS_ERROR;
                        backends[i].error = status + BACKEND_ERROR_HTTP_STATUS_BASE;
                        strlcpy(backends[i].message, (char *)http_response_buffer, sizeof(backends[i].message));
                        if(status >= 300)
                            ESP_LOGI(__func__, "HTTP Error %i: %s", status, http_response_buffer);
                        else if(http_response_length && backends[i].format == BACKEND_FORMAT_POSTMAN && backends[i].auth == BACKEND_AUTH_POSTMAN) {
                            hmac_sha256_key_t binary_key;
                            if(hmac_hex_decode(binary_key, sizeof(binary_key), backends[i].key, strlen(backends[i].key)) == sizeof(binary_key))
                                postman_handle_pack(&postman,
                                    (bp_type_t *) http_response_buffer,
                                    http_response_length / sizeof(bp_type_t),
                                    sizeof(http_response_buffer) / sizeof(bp_type_t),
                                    NOW, backends[i].user, binary_key);
                            else
                                ESP_LOGI(__func__, "HMAC key is not 64 bytes long");
                        }
                    }
                    else {
                        backends[i].status = BACKEND_STATUS_ERROR;
                        backends[i].error = err;
                        backends[i].message[0] = 0;
                    }
                    esp_http_client_cleanup(client);
                    break;
                }
                case 'm':   // mqtt / mqtts
                    size_t backend_buffer_length;
                    if(backends_started && (backend_buffer_length = encode_measurements(i)) != 0) {
                        err = esp_mqtt_client_publish(backends[i].handle, backends[i].output_topic, backend_buffer, backend_buffer_length, 0, 0);
                        backends[i].status = err < 0 ? BACKEND_STATUS_ERROR : BACKEND_STATUS_ONLINE;
                        backends[i].error = err;
                        backends[i].message[0] = 0;
                        ESP_LOGI(__func__, "esp_mqtt_client_publish: %s", err < 0 ? "failed" : "done");
                    }
                    break;
                case 'u':   // udp
                    measurements_index_t index = 0;
                    measurements_index_t count = measurements_full ? MEASUREMENTS_NUM_MAX : measurements_count;

                    struct yuarel url;
                    char url_string[BACKEND_URI_LENGTH];
                    strlcpy(url_string, backends[i].uri, BACKEND_URI_LENGTH);
                    if (yuarel_parse(&url, url_string)) {
                        backends[i].status = BACKEND_STATUS_ERROR;
                        backends[i].error = ESP_ERR_INVALID_ARG;
                        strlcpy(backends[i].message, "Parsing the URI failed", sizeof(backends[i].message));
                        ESP_LOGE(__func__, "Parsing the URI failed: %s", url_string);
                        break;
                    }

                    int    sock;
                    void   *addr;
                    size_t addr_size;
                    struct sockaddr_in  addr4 = {0};
                    struct sockaddr_in6 addr6 = {0};

                    if(url.host[0] != '[') {
                        addr4.sin_addr.s_addr = inet_addr(url.host);
                        addr4.sin_family = AF_INET;
                        addr4.sin_port = htons(url.port);
                        addr = &addr4;
                        addr_size = sizeof(addr4);
                        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
                    }
                    else {
                        url.host += 1;
                        url.host[strlen(url.host) - 1] = 0;
                        inet6_aton(url.host, &addr6.sin6_addr);
                        addr6.sin6_family = AF_INET6;
                        addr6.sin6_port = htons(url.port);
                        addr6.sin6_scope_id = esp_netif_get_netif_impl_index(wifi.netif);
                        addr = &addr6;
                        addr_size = sizeof(addr6);
                        sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_IPV6);
                    }
                    if (sock < 0) {
                        backends[i].status = BACKEND_STATUS_ERROR;
                        backends[i].error = errno;
                        strlcpy(backends[i].message, "Unable to create socket", sizeof(backends[i].message));
                        ESP_LOGE(__func__, "Unable to create socket: errno %d", errno);
                        break;
                    }

                    for(int n = 0; n < count; n++) {
                        pbuf_t buf = { backend_buffer, sizeof(backend_buffer), 0 };
                        index = measurements_full ? (measurements_count + n) % MEASUREMENTS_NUM_MAX : n;

                        switch(backends[i].format) {
                        case BACKEND_FORMAT_SENML:
                            measurements_entry_to_senml_row(index, &buf);
                            break;
                        case BACKEND_FORMAT_POSTMAN:
                            buf.length = buf.size;
                            measurements_entry_to_postman(index, buf.data, &buf.length,
                                backends[i].auth == BACKEND_AUTH_POSTMAN ? backends[i].user : NULL,
                                backends[i].auth == BACKEND_AUTH_POSTMAN ? backends[i].key : NULL);
                            break;
                        case BACKEND_FORMAT_TEMPLATE:
                            measurements_entry_to_template_row(index, &buf, backends[i].template_row, backends[i].template_name_separator);
                            break;
                        case BACKEND_FORMAT_BINARY:
                            buf.length = sizeof(measurement_frame_t);
                            measurements_entry_to_frame(index, (measurement_frame_t *) buf.data);
                            break;
                        default:
                            backends[i].status = BACKEND_STATUS_ERROR;
                            backends[i].error = ESP_ERR_INVALID_ARG;
                            strlcpy(backends[i].message, "Unsupported format", sizeof(backends[i].message));
                            ESP_LOGE(__func__, "Unsupported format at backend %i", i);
                            break;
                        }
                        if(buf.length) {
                            err = sendto(sock, buf.data, buf.length, 0, (struct sockaddr *) addr, addr_size);
                            ESP_LOGI(__func__, "sent measurement %i via UDP: %s %i", index, err < 0 ? "failed" : "done", err);
                        }
                    }
                    close(sock);
                    if(application.sleep)
                        vTaskDelay (100 / portTICK_PERIOD_MS); // wait for WiFi TX pending packets to be sent, not sure about the 100ms
                    break;
                default:
                    backends[i].status = BACKEND_STATUS_ERROR;
                    backends[i].error = ESP_ERR_INVALID_ARG;
                    strlcpy(backends[i].message, "Unknown protocol", sizeof(backends[i].message));
                    break;
                }
                ESP_LOGI(__func__, "finished sending measurements @ %lli", esp_timer_get_time());
            }
            measurements_updated = false;
            ready_to_sleep = true;
        }

        now = esp_timer_get_time();
        if(application.sleep &&
          (ready_to_sleep || (measurements_updated && now - application.last_measurement_time > 10 * 1000000)) &&
          (slept_once || now > 60 * 1000000)) {
            ready_to_sleep = false;
            int64_t sleep_duration = application.next_measurement_time - now - (ble.enabled ? ble.scan_duration * 1000000 : 0);
            if(sleep_duration > 0) {
                slept_once = true;
                wifi_stop();
                i2c_set_power(false);
                onewire_set_power(false);
                esp_sleep_enable_timer_wakeup(sleep_duration);
                esp_deep_sleep_start();
                // this is never reached
            }
        }

        vTaskDelay (10 / portTICK_PERIOD_MS);
    }
}

