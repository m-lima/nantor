#define DT_DRV_COMPAT zmk_behavior_status_led

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(zmk_behavior_status_led) == 1,
             "Must have exactly one status-led behavior node");
#define NODE DT_INST(0, zmk_behavior_status_led)
#define LED_COUNT DT_PROP_LEN(NODE, led_gpios)
BUILD_ASSERT(
    LED_COUNT == 3,
    "Must have exactly three GPIO pins for the status-led behavior node");

typedef enum {
  OFF = 0,
  LOW = 1,
  HIGH = 2,
  FULL = 3,
} mode;

struct status_led_command {
  mode led0 : 2;
  mode led1 : 2;
  mode led2 : 2;
};

K_MSGQ_DEFINE(status_led_msgq, sizeof(struct status_led_command), 4, 1);

static const struct gpio_dt_spec led_gpios[LED_COUNT] = {
    GPIO_DT_SPEC_GET_BY_IDX(NODE, led_gpios, 0),
    GPIO_DT_SPEC_GET_BY_IDX(NODE, led_gpios, 1),
    GPIO_DT_SPEC_GET_BY_IDX(NODE, led_gpios, 2),
};

int behavior_status_led_ble_listener(const zmk_event_t *eh) {
  struct status_led_command command = {
      .led0 = LOW,
      .led1 = HIGH,
      .led2 = FULL,
  };

  k_msgq_put(&status_led_msgq, &command, K_NO_WAIT);
  return ZMK_EV_EVENT_BUBBLE;
}

#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_LISTENER(status_led_ble, behavior_status_led_ble_listener);
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
ZMK_SUBSCRIPTION(status_led_ble, zmk_ble_active_profile_changed);
#elif IS_ENABLED(CONFIG_ZMK_SPLIT_BLE)
ZMK_SUBSCRIPTION(status_led_ble, zmk_split_peripheral_status_changed);
#endif
#endif // IS_ENABLED(CONFIG_ZMK_BLE)

static int behavior_status_led_init(const struct device *dev) {
  for (int i = 0; i < 3; ++i) {
    if (!device_is_ready(led_gpios[i].port)) {
      return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&led_gpios[i], GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
      return ret;
    }

    // Turn on
    gpio_pin_set_dt(&led_gpios[0], 1);
    gpio_pin_set_dt(&led_gpios[1], 1);
    gpio_pin_set_dt(&led_gpios[2], 1);
  }

  return 0;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
  gpio_pin_set_dt(&led_gpios[binding->param1], 1);

  return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
  gpio_pin_set_dt(&led_gpios[binding->param1], 0);

  return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_status_led_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

BEHAVIOR_DT_INST_DEFINE(0, behavior_status_led_init, NULL, NULL, NULL,
                        POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_status_led_driver_api);

#endif
