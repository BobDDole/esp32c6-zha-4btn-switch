#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_zigbee_core.h"
#include "esp_zigbee_zcl.h"

#define TAG "ZHA_4BTN"

/* GPIOs for buttons */
#define BTN1_GPIO 4
#define BTN2_GPIO 5
#define BTN3_GPIO 6
#define BTN4_GPIO 7

#define LONG_PRESS_MS   800
#define DOUBLE_PRESS_MS 400

typedef struct {
    uint32_t last_press_time;
    uint8_t click_count;
    bool pressed;
} button_state_t;

static button_state_t buttons[4];

static void send_zigbee_button_event(uint8_t endpoint, uint8_t press_type)
{
    esp_zb_zcl_set_attribute_val(
        endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_MULTI_STATE_INPUT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_MULTI_STATE_INPUT_PRESENT_VALUE_ID,
        &press_type,
        false);

    ESP_LOGI(TAG, "EP %d event %d", endpoint, press_type);
}

static void button_task(void *arg)
{
    while (1) {
        uint32_t now = esp_log_timestamp();

        for (int i = 0; i < 4; i++) {
            gpio_num_t gpio = BTN1_GPIO + i;
            bool level = gpio_get_level(gpio) == 0; // active low

            if (level && !buttons[i].pressed) {
                buttons[i].pressed = true;
                buttons[i].last_press_time = now;
            }

            if (!level && buttons[i].pressed) {
                uint32_t press_time = now - buttons[i].last_press_time;
                buttons[i].pressed = false;

                if (press_time > LONG_PRESS_MS) {
                    send_zigbee_button_event(i + 1, 3);
                } else {
                    buttons[i].click_count++;
                }
            }

            if (buttons[i].click_count > 0 &&
                (now - buttons[i].last_press_time) > DOUBLE_PRESS_MS) {

                if (buttons[i].click_count == 1) {
                    send_zigbee_button_event(i + 1, 1);
                } else {
                    send_zigbee_button_event(i + 1, 2);
                }
                buttons[i].click_count = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void zigbee_init(void)
{
    esp_zb_cfg_t zb_config = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_config);

    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();

    for (uint8_t ep = 1; ep <= 4; ep++) {
        esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
        esp_zb_zcl_add_basic_cluster(cluster_list, NULL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        esp_zb_zcl_add_identify_cluster(cluster_list, NULL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        esp_zb_zcl_add_multistate_input_cluster(cluster_list, NULL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

        esp_zb_endpoint_config_t ep_config = {
            .endpoint = ep,
            .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
            .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID,
            .app_device_version = 0,
        };

        esp_zb_ep_list_add_ep(ep_list, cluster_list, &ep_config);
    }

    esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);

    esp_zb_set_manufacturer_name("DIY");
    esp_zb_set_model_identifier("C6-4BTN-ZHA");
    esp_zb_set_application_version(1);

    esp_zb_device_register(ep_list);
    esp_zb_start(false);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask =
            (1ULL << BTN1_GPIO) |
            (1ULL << BTN2_GPIO) |
            (1ULL << BTN3_GPIO) |
            (1ULL << BTN4_GPIO),
    };
    gpio_config(&io_conf);

    zigbee_init();

    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
}
