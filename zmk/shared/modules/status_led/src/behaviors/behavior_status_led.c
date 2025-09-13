#define DT_DRV_COMPAT zmk_behavior_status_led

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>

// #if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
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

ZMK_LISTENER(status_led_ble, behavior_status_led_ble_listener);
ZMK_SUBSCRIPTION(status_led_ble, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(status_led_ble, zmk_split_peripheral_status_changed);

// static int behavior_status_led_init(const struct device *dev) {
//   const struct behavior_status_led_config *config = dev->config;
//   int err = 0;
//
//   if (config->num_leds != 3) {
//     err = -EINVAL;
//   }
//
//   return err;
// }
//
// static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
//                                      struct zmk_behavior_binding_event
//                                      event) {
//   // // #if IS_ENABLED(CONFIG_RGBLED_WIDGET)
//   // const struct device *dev =
//   zmk_behavior_get_binding(binding->behavior_dev);
//   // const struct behavior_rgb_wdg_config *cfg = dev->config;
//   //
//   // // #if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
//   // if (cfg->indicate_battery) {
//   //   indicate_battery();
//   // }
//   // // #endif
//   // // #if IS_ENABLED(CONFIG_ZMK_USB) || IS_ENABLED(CONFIG_ZMK_BLE)
//   // if (cfg->indicate_connectivity) {
//   //   indicate_connectivity();
//   // }
//   // // #endif
//   // // #if !IS_ENABLED(CONFIG_ZMK_SPLIT) ||
//   // // IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
//   // if (cfg->indicate_layer) {
//   //   indicate_layer();
//   // }
//   // // #endif
//   // // #endif // IS_ENABLED(CONFIG_RGBLED_WIDGET)
//
//   return ZMK_BEHAVIOR_OPAQUE;
// }
//
// static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
//                                       struct zmk_behavior_binding_event
//                                       event) {
//   return ZMK_BEHAVIOR_OPAQUE;
// }
//
// static const struct behavior_driver_api behavior_status_led_driver_api = {
//     .binding_pressed = on_keymap_binding_pressed,
//     .binding_released = on_keymap_binding_released,
//     .locality = BEHAVIOR_LOCALITY_GLOBAL,
// #if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
//     .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
// #endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
// };
//
// // #endif
