#define DT_DRV_COMPAT zmk_behavior_status_led

#include <drivers/behavior.h>
#include <zephyr/device.h>

#include <zmk/behavior.h>

// #if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
struct behavior_status_led_config {
  // bool indicate_battery;
  // bool indicate_connectivity;
  // bool indicate_layer;
};

static int behavior_status_led_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
  // // #if IS_ENABLED(CONFIG_RGBLED_WIDGET)
  // const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
  // const struct behavior_rgb_wdg_config *cfg = dev->config;
  //
  // // #if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
  // if (cfg->indicate_battery) {
  //   indicate_battery();
  // }
  // // #endif
  // // #if IS_ENABLED(CONFIG_ZMK_USB) || IS_ENABLED(CONFIG_ZMK_BLE)
  // if (cfg->indicate_connectivity) {
  //   indicate_connectivity();
  // }
  // // #endif
  // // #if !IS_ENABLED(CONFIG_ZMK_SPLIT) ||
  // // IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  // if (cfg->indicate_layer) {
  //   indicate_layer();
  // }
  // // #endif
  // // #endif // IS_ENABLED(CONFIG_RGBLED_WIDGET)

  return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
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

// #endif
