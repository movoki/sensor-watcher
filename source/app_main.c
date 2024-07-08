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
#include <esp_tls.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <mqtt_client.h>

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32S3)
    #define USE_USB_SERIAL_JTAG
#endif
#ifdef USE_USB_SERIAL_JTAG
    #include <driver/usb_serial_jtag.h>
    #include <hal/usb_serial_jtag_ll.h>
#endif
#ifdef USE_TINYUSB
    #include <tinyusb.h>
    #include <tusb_cdc_acm.h>
#endif

#include "adc.h"
#include "application.h"
#include "ble.h"
#include "board.h"
#include "backends.h"
#include "devices.h"
#include "enums.h"
#include "framer.h"
#include "httpdate.h"
#include "i2c.h"
#include "logs.h"
#include "measurements.h"
#include "nodes.h"
#include "now.h"
#include "onewire.h"
#include "postman.h"
#include "schema.h"
#include "wifi.h"
#include "yuarel.h"

#define POSTMAN_PACKET_LENGTH_MAX       (9 * 1024)                      // To fit a full packed backend_t plus headers
#define UART_BUFFER_SIZE                POSTMAN_PACKET_LENGTH_MAX
#define UART_NUMBER                     UART_NUM_0
#define USB_SERIAL_JTAG_BUFFER_SIZE     1024

framer_t framer;
postman_t postman;

uint32_t postman_buffer[POSTMAN_PACKET_LENGTH_MAX / 4];

size_t backend_buffer_length = 0;
alignas(4) char backend_buffer[POSTMAN_PACKET_LENGTH_MAX];

time_t http_timestamp = 0;

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
#if defined(USE_TINYUSB)
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
#elif defined(USE_USB_SERIAL_JTAG)
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .rx_buffer_size = USB_SERIAL_JTAG_BUFFER_SIZE,
        .tx_buffer_size = USB_SERIAL_JTAG_BUFFER_SIZE,
    };

    err = err ? err : usb_serial_jtag_driver_install(&usb_serial_jtag_config);
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
#if defined(USE_TINYUSB)
    return tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, bytes, length);
#elif defined(USE_USB_SERIAL_JTAG)
    return usb_serial_jtag_write_bytes((const char *) bytes, length, 0);
#else
    return uart_write_bytes(0, bytes, length);
#endif
}

int serial_read_bytes(uint8_t *bytes, size_t length)
{
#if defined(USE_TINYUSB)
    return tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0, bytes, length, &length) == ESP_OK ? length : 0;
#elif defined(USE_USB_SERIAL_JTAG)
    return usb_serial_jtag_read_bytes(bytes, length, 0);
#else
    return uart_read_bytes(0, bytes, length, 0);
#endif
}

void serial_flush()
{
#if defined(USE_TINYUSB)
    tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
#elif defined(USE_USB_SERIAL_JTAG)
    usb_serial_jtag_ll_txfifo_flush();
#endif
}

void serial_send_receive()
{
    uint8_t byte;

    #if defined(USE_TINYUSB)
        int n;
        for(n = 0; framer.state == FRAMER_SENDING && n < 512; n++) {
            byte = framer_get_byte_to_send(&framer);
            serial_write_bytes(&byte, 1);
        }
        serial_flush();
        if(n == 512)
            vTaskDelay(60 / portTICK_PERIOD_MS);
    #elif defined(USE_USB_SERIAL_JTAG)
        for(int n = 0; framer.state == FRAMER_SENDING && n < USB_SERIAL_JTAG_BUFFER_SIZE; n++) {
            byte = framer_get_byte_to_send(&framer);
            serial_write_bytes(&byte, 1);
        }
        serial_flush();
    #else
        while(framer.state == FRAMER_SENDING) {
            byte = framer_get_byte_to_send(&framer);
            serial_write_bytes(&byte, 1);
        }
    #endif

    while(framer.state == FRAMER_RECEIVING && serial_read_bytes(&byte, 1)) {
        if(framer_put_received_byte(&framer, byte) && framer.length) {
            framer.length = postman_handle_pack(&postman, postman_buffer, framer.length / 4, sizeof(postman_buffer) / 4, 0, NULL, NULL) * 4;
            framer_set_state(&framer, FRAMER_SENDING);
            break;
        }
    }
}

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    switch(event->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
             backend_buffer_length = 0;
             break;
        case HTTP_EVENT_ON_HEADER:
            if(!strncmp(event->header_key, "Date", 5))
                httpdate_parse(event->header_value, &http_timestamp);
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(event->client)) {
                size_t copy_len = MIN(event->data_len, (sizeof(backend_buffer) - 1 - backend_buffer_length));
                if(copy_len)
                    memcpy(backend_buffer + backend_buffer_length, event->data, copy_len);
                backend_buffer_length += copy_len;
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
    size_t length = sizeof(backend_buffer);

    switch(backends[backend_index].format) {
        case BACKEND_FORMAT_SENML:
            ok = ok && measurements_to_senml(backend_buffer, &length);
            break;
        case BACKEND_FORMAT_POSTMAN:
            ok = ok && measurements_to_postman(backend_buffer, &length,
                backends[backend_index].auth == BACKEND_AUTH_POSTMAN ? backends[backend_index].user : NULL,
                backends[backend_index].auth == BACKEND_AUTH_POSTMAN ? backends[backend_index].key : NULL);
            break;
        case BACKEND_FORMAT_TEMPLATE:
            ok = ok && measurements_to_template(backend_buffer, &length,
                backends[backend_index].template_header, backends[backend_index].template_row,
                backends[backend_index].template_row_separator, backends[backend_index].template_path_separator,
                backends[backend_index].template_footer);
            break;
        default:
            ok = false;
    }

    ESP_LOGI(__func__, "backend buffer length / size: %u / %u", length, sizeof(backend_buffer));

    if(!ok && !length)
        ESP_LOGE(__func__, "backend buffer overflow!");

    if(!ok) {
        backends[backend_index].status = BACKEND_STATUS_ERROR;
        backends[backend_index].error = 0x201; // Serialization failed
        backends[backend_index].message[0] = 0;
        length = 0;
    }
    return length;
}

void app_main(void)
{
    esp_err_t err;
    int64_t now;
    bool ready_to_sleep = false;
    bool measurements_updated = false;

    esp_event_loop_create_default();

    struct timeval zero_time = { .tv_sec = 0 };   // Reset system time to avoid using the unreliable internal RTC
    settimeofday(&zero_time, NULL);

    logs_init();        // order of inits is important!
    serial_init();
    nvs_init();         // 100 ms
    board_init();
    wifi_init();        // 160 ms
    application_init();
    nodes_init();
    backends_init(); // 47 ms
    measurements_init();
    adc_init();
    ble_init();

    if(!slept_once)
        devices_init();
    else
        devices_buses_start();

    framer_set_buffer(&framer, (uint8_t *)postman_buffer, sizeof(postman_buffer));
    postman_init(&postman);
    postman_register_resource(&postman, "@", &schema_resource_handler);
    postman_register_resource(&postman, "adc", &adc_resource_handler);
    postman_register_resource(&postman, "application", &application_resource_handler);
    postman_register_resource(&postman, "ble", &ble_resource_handler);
    postman_register_resource(&postman, "board", &board_resource_handler);
    postman_register_resource(&postman, "backends", &backends_resource_handler);
    postman_register_resource(&postman, "devices", &devices_resource_handler);
    postman_register_resource(&postman, "i2c", &i2c_resource_handler);
    postman_register_resource(&postman, "logs", &logs_resource_handler);
    postman_register_resource(&postman, "measurements", &measurements_resource_handler);
    postman_register_resource(&postman, "nodes", &nodes_resource_handler);
    postman_register_resource(&postman, "onewire", &onewire_resource_handler);
    postman_register_resource(&postman, "wifi", &wifi_resource_handler);

    if(ble.receive && ble.scan_duration != 0xFF)
        application.next_measurement_time += ble.scan_duration * 1000000;

    ESP_LOGI(__func__, "inits ended @ %lli", esp_timer_get_time());
    ESP_LOGI(__func__, "application.next_measurement_time: %lli", application.next_measurement_time);

    ESP_LOGI(__func__, "sizeof devices: %u", sizeof(device_t) * DEVICES_NUM_MAX);
    ESP_LOGI(__func__, "sizeof measurements: %u", sizeof(measurement_t) * MEASUREMENTS_NUM_MAX);
    ESP_LOGI(__func__, "sizeof backends: %u", sizeof(backend_t) * BACKENDS_NUM_MAX);

    while(true) {
        serial_send_receive();

        if(!sntp_started && wifi.status == WIFI_STATUS_ONLINE && !application.sleep) {
            esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
            esp_sntp_setservername(0, "pool.ntp.org");
            esp_sntp_init();
            sntp_started = true;
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

        if(ble.receive && !ble_is_scanning() && (ble.scan_duration == 0xFF
          || esp_timer_get_time() + ble.scan_duration * 1000000LL >= application.next_measurement_time)) {
            ble_start_scan();
            ESP_LOGI(__func__, "starting ble scan @ %lli", esp_timer_get_time());
        }

        now = esp_timer_get_time();
        if(now >= application.next_measurement_time) {
            ESP_LOGI(__func__, "starting measurements @ %lli", now);
            application.last_measurement_time = now;
            application.next_measurement_time += application.sampling_period * 1000000L;
            ESP_LOGI(__func__, "last_measurement_time %lli next_measurement_time %lli", (long long int)application.last_measurement_time, (long long int)application.next_measurement_time);
            if(!application.queue)
                measurements_init();
            measurements_measure();
            // stop the scan if not in continuous mode or there are BLE measurements
            if(ble.receive && (ble.scan_duration != 0xFF || ble_measurements_count)) {
                ble_stop_scan();
                ESP_LOGI(__func__, "ble_measurements_count: %lu", ble_measurements_count);
                ble_merge_measurements();
            }
            if(!application.queue && measurements_full)
                ESP_LOGE(__func__, "measurements buffer overflow!");
            measurements_updated = true;
            ESP_LOGI(__func__, "finished measurements @ %lli", esp_timer_get_time());
        }

        if(ble.send && measurements_updated) {
            ESP_LOGI(__func__, "started sending measurements via BLE @ %lli", esp_timer_get_time());
            ble_send_measurements();
            if(!wifi.ssid[0]) {
                measurements_updated = false;
                ready_to_sleep = true;
            }
            ESP_LOGI(__func__, "finished sending measurements via BLE @ %lli", esp_timer_get_time());
        }

        if(wifi.status == WIFI_STATUS_ONLINE && ((measurements_updated && (measurements_count || measurements_full)) || backends_modified)) {
            wifi_measure();
            for(uint8_t i = 0; i != BACKENDS_NUM_MAX; i++) {
                if(backends[i].uri[0] == 0 || (backends_modified && !(backends_modified & 1 << i)))
                    continue;

                ESP_LOGI(__func__, "started sending measurements via WiFi @ %lli", esp_timer_get_time());
                switch(backends[i].uri[0]) {
                case 'h': {     // http / https
                    esp_http_client_config_t config_post = {
                        .url = backends[i].uri,
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

                    if(!NOW && (backends[i].auth == BACKEND_AUTH_POSTMAN || strstr(backends[i].template_row, "@t"))) {
                        http_timestamp = 0;
                        backend_buffer_length = 0;
                        esp_http_client_set_method(client, HTTP_METHOD_HEAD);
                        err = esp_http_client_perform(client);
                        if(err == ESP_OK) {
                            struct timeval now = { .tv_sec = http_timestamp };
                            settimeofday(&now, NULL);
                            ESP_LOGI(__func__, "System time set to HTTP Date: %lli", http_timestamp);
                        }
                        else {
                            backends[i].status = BACKEND_STATUS_ERROR;
                            backends[i].error = err;
                            backends[i].message[0] = 0;
                            continue;
                        }
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

                    backend_buffer_length = encode_measurements(i);
                    if(!backend_buffer_length)
                        continue;

                    esp_http_client_set_method(client, HTTP_METHOD_POST);
                    esp_http_client_set_post_field(client, backend_buffer, backend_buffer_length);

                    backend_buffer_length = 0;
                    err = esp_http_client_perform(client);
                    if(err == ESP_OK) {
                        int status = esp_http_client_get_status_code(client);
                        backends[i].status = status < 300 ? BACKEND_STATUS_ONLINE : BACKEND_STATUS_ERROR;
                        backends[i].error = status + BACKEND_ERROR_HTTP_STATUS_BASE;
                        backend_buffer[backend_buffer_length] = 0;
                        strlcpy(backends[i].message, (char *)backend_buffer, sizeof(backends[i].message));

                        if(status >= 300)
                            ESP_LOGI(__func__, "HTTP Error %i: %s", status, backend_buffer);
                        else if(backend_buffer_length && backends[i].format == BACKEND_FORMAT_POSTMAN && backends[i].auth == BACKEND_AUTH_POSTMAN) {
                            hmac_sha256_key_t binary_key;
                            if(hmac_hex_decode(binary_key, sizeof(binary_key), backends[i].key, strlen(backends[i].key)) == sizeof(binary_key)) {
                                ESP_LOGI(__func__, "Handling HTTP Postman request");
                                backend_buffer_length = sizeof(bp_type_t) * postman_handle_pack(&postman,
                                    (bp_type_t *) backend_buffer,
                                    backend_buffer_length / sizeof(bp_type_t),
                                    sizeof(backend_buffer) / sizeof(bp_type_t),
                                    NOW, backends[i].user, binary_key);
                                ESP_LOGI(__func__, "HTTP Postman response: buffer length %u", backend_buffer_length);
                                if(backend_buffer_length) {
                                    esp_http_client_set_post_field(client, backend_buffer, backend_buffer_length);
                                    backend_buffer_length = 0;
                                    err = esp_http_client_perform(client);
                                    status = esp_http_client_get_status_code(client);
                                    ESP_LOGI(__func__, "HTTP Postman response: err %i status %i",err,status);
                                }
                            }
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
                            measurements_entry_to_template_row(index, &buf, backends[i].template_row, backends[i].template_path_separator);
                            break;
                        case BACKEND_FORMAT_FRAME:
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
                ESP_LOGI(__func__, "finished sending measurements via WiFi @ %lli", esp_timer_get_time());
            }
            backends_modified = 0;
            measurements_updated = false;
            ready_to_sleep = true;
        }

        now = esp_timer_get_time();
        if(application.sleep && framer.state != FRAMER_SENDING &&
          (ready_to_sleep || (measurements_updated && now - application.last_measurement_time > 10 * 1000000)) &&
          (slept_once || now > 60 * 1000000)) {
            ready_to_sleep = false;
            int64_t sleep_duration = application.next_measurement_time - now - (ble.receive ? ble.scan_duration * 1000000 : 0);
            if(sleep_duration > 0) {
                slept_once = true;
                wifi_stop();
                ble_stop();
                i2c_stop();
                onewire_stop();
                board_stop();
                esp_sleep_enable_timer_wakeup(sleep_duration);
                esp_deep_sleep_start();
                // this is never reached
            }
        }

        vTaskDelay (20 / portTICK_PERIOD_MS);
    }
}

