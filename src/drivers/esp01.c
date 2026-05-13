#include "drivers/esp01.h"
#include "hal/uart.h"
#include "hal/timer.h"
#include "app_config.h"

#include <stdio.h>
#include <string.h>

#define ESP_LINE_MAX 64
#define ESP_REQ_MAX  128

typedef enum {
    ESP_STATE_INIT = 0,
    ESP_STATE_RESET,
    ESP_STATE_WAIT_READY,
    ESP_STATE_ECHO_OFF,
    ESP_STATE_CWMODE,
    ESP_STATE_CWJAP,
    ESP_STATE_WAIT_IP,
    ESP_STATE_IDLE,
    ESP_STATE_CIPSTART,
    ESP_STATE_CIPSEND,
    ESP_STATE_WAIT_PROMPT,
    ESP_STATE_SEND_DATA,
    ESP_STATE_WAIT_HTTP,
    ESP_STATE_CIPCLOSE,
    ESP_STATE_ERROR_BACKOFF
} esp_state_t;

static esp_state_t s_state;
static uint32_t    s_state_timer;
static uint32_t    s_timeout_ms;

static char        s_line_buf[ESP_LINE_MAX];
static uint8_t     s_line_len;

static bool        s_flag_ok;
static bool        s_flag_error;
static bool        s_flag_ready;
static bool        s_flag_got_ip;
static bool        s_flag_prompt;   /* '>' */
static bool        s_flag_closed;

static uint8_t     s_post_temp;
static uint8_t     s_post_humi;
static char        s_req_buf[ESP_REQ_MAX];
static uint16_t    s_req_len;

static const char* s_state_names[] = {
    "ESP: Init", "ESP: Resetting", "ESP: Booting", "ESP: Echo Off",
    "ESP: Config", "ESP: Joining", "ESP: Wait IP", "ESP: Idle (WiFi OK)",
    "ESP: TCP Connect", "ESP: TCP Send", "ESP: Wait >", "ESP: Uploading",
    "ESP: Wait Reply", "ESP: Closing", "ESP: Error/Backoff"
};

static void change_state(esp_state_t next, uint32_t timeout)
{
    s_state = next;
    s_state_timer = millis();
    s_timeout_ms = timeout;
    s_flag_ok = s_flag_error = s_flag_ready = s_flag_got_ip = s_flag_prompt = s_flag_closed = false;
}

static void send_cmd(const char *cmd)
{
    uart_flush_rx();
    uart_puts(cmd);
}

void esp01_init(void)
{
    s_line_len = 0;
    change_state(ESP_STATE_INIT, 1000);
}

void esp01_post(uint8_t temp, uint8_t humidity)
{
    if (s_state == ESP_STATE_IDLE) {
        s_post_temp = temp;
        s_post_humi = humidity;
        
        s_req_len = (uint16_t)snprintf(s_req_buf, sizeof(s_req_buf),
            "GET %s&field1=%u&field2=%u HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n\r\n",
            TELEMETRY_PATH, s_post_temp, s_post_humi, TELEMETRY_HOST);
            
        change_state(ESP_STATE_CIPSTART, 5000);
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", TELEMETRY_HOST, TELEMETRY_PORT);
        send_cmd(cmd);
    }
}

const char* esp01_get_state_str(void)
{
    if (s_state <= ESP_STATE_ERROR_BACKOFF) {
        return s_state_names[s_state];
    }
    return "ESP: Unknown";
}

static void process_line(void)
{
    if (s_line_len == 0) return;
    s_line_buf[s_line_len] = '\0';

    if (strstr(s_line_buf, "OK"))           s_flag_ok = true;
    if (strstr(s_line_buf, "ERROR"))        s_flag_error = true;
    if (strstr(s_line_buf, "FAIL"))         s_flag_error = true;
    if (strstr(s_line_buf, "ready"))        s_flag_ready = true;
    if (strstr(s_line_buf, "WIFI GOT IP"))  s_flag_got_ip = true;
    if (strstr(s_line_buf, "CLOSED"))       s_flag_closed = true;

    s_line_len = 0;
}

void esp01_tick(void)
{
    int ch;
    while ((ch = uart_getc()) >= 0) {
        char c = (char)ch;
        if (c == '\n' || c == '\r') {
            process_line();
        } else if (c == '>') {
            s_flag_prompt = true;
        } else if (s_line_len < ESP_LINE_MAX - 1 && c >= 0x20 && c < 0x7F) {
            s_line_buf[s_line_len++] = c;
        }
    }

    bool timeout = ((uint32_t)(millis() - s_state_timer) >= s_timeout_ms);

    if (timeout && s_state != ESP_STATE_IDLE && s_state != ESP_STATE_INIT) {
        change_state(ESP_STATE_ERROR_BACKOFF, 10000);
        return;
    }

    switch (s_state) {
        case ESP_STATE_INIT:
            if (timeout) {
                change_state(ESP_STATE_RESET, 2000);
                send_cmd("AT+RST\r\n");
            }
            break;

        case ESP_STATE_RESET:
            if (s_flag_ok || s_flag_ready || timeout) {
                change_state(ESP_STATE_WAIT_READY, 3000);
            }
            break;

        case ESP_STATE_WAIT_READY:
            if (s_flag_ready || timeout) {
                change_state(ESP_STATE_ECHO_OFF, 2000);
                send_cmd("ATE0\r\n");
            }
            break;

        case ESP_STATE_ECHO_OFF:
            if (s_flag_ok || timeout) {
                change_state(ESP_STATE_CWMODE, 2000);
                send_cmd("AT+CWMODE=1\r\n");
            }
            break;

        case ESP_STATE_CWMODE:
            if (s_flag_ok) {
                change_state(ESP_STATE_CWJAP, 15000);
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASS);
                send_cmd(cmd);
            } else if (s_flag_error) {
                change_state(ESP_STATE_ERROR_BACKOFF, 5000);
            }
            break;

        case ESP_STATE_CWJAP:
            if (s_flag_ok || s_flag_got_ip) {
                change_state(ESP_STATE_IDLE, 0); /* No timeout in IDLE */
            } else if (s_flag_error) {
                change_state(ESP_STATE_ERROR_BACKOFF, 10000);
            }
            break;

        case ESP_STATE_IDLE:
            /* Waiting for esp01_post() to be called */
            break;

        case ESP_STATE_CIPSTART:
            if (s_flag_ok) {
                change_state(ESP_STATE_CIPSEND, 3000);
                char cmd[32];
                snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", s_req_len);
                send_cmd(cmd);
            } else if (s_flag_error || s_flag_closed) {
                change_state(ESP_STATE_ERROR_BACKOFF, 5000);
            }
            break;

        case ESP_STATE_CIPSEND:
        case ESP_STATE_WAIT_PROMPT:
            if (s_flag_prompt || s_flag_ok) {
                change_state(ESP_STATE_SEND_DATA, 10000);
                uart_puts(s_req_buf);
            } else if (s_flag_error) {
                change_state(ESP_STATE_ERROR_BACKOFF, 3000);
            }
            break;

        case ESP_STATE_SEND_DATA:
        case ESP_STATE_WAIT_HTTP:
            if (s_flag_closed || s_flag_ok || s_flag_error) {
                change_state(ESP_STATE_CIPCLOSE, 2000);
                send_cmd("AT+CIPCLOSE\r\n");
            }
            break;

        case ESP_STATE_CIPCLOSE:
            if (s_flag_ok || s_flag_error || timeout) {
                change_state(ESP_STATE_IDLE, 0);
            }
            break;

        case ESP_STATE_ERROR_BACKOFF:
            if (timeout) {
                change_state(ESP_STATE_INIT, 1000);
            }
            break;

        default:
            change_state(ESP_STATE_INIT, 1000);
            break;
    }
}