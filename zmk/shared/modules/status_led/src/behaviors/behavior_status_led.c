#define DT_DRV_COMPAT zmk_behavior_status_led

#include <drivers/behavior.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include <zmk/battery.h>
#include <zmk/behavior.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/split/bluetooth/peripheral.h>

#include <dt-bindings/zmk/stled.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

/*
 * GPIO initialization
 */
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(zmk_behavior_status_led) == 1,
             "Must have exactly one status-led behavior node");
#define NODE DT_INST(0, zmk_behavior_status_led)
#define LED_COUNT DT_PROP_LEN(NODE, led_gpios)
BUILD_ASSERT(
    LED_COUNT == 3,
    "Must have exactly three GPIO pins for the status-led behavior node");

static const struct gpio_dt_spec led_gpios[LED_COUNT] = {
    GPIO_DT_SPEC_GET_BY_IDX(NODE, led_gpios, 0),
    GPIO_DT_SPEC_GET_BY_IDX(NODE, led_gpios, 1),
    GPIO_DT_SPEC_GET_BY_IDX(NODE, led_gpios, 2),
};

/*
 * LED command definition
 */
typedef enum {
  OFF = 0,
  FULL = 1,
  LOW = 2,
  HIGH = 3,
} mode;

struct behavior_status_led_command {
  mode led0 : 2;
  mode led1 : 2;
  mode led2 : 2;
};

inline static void
make_channel_command(struct behavior_status_led_command *command, mode mode,
                     uint8_t channel) {
  command->led0 = (channel & 1 ? mode : OFF);
  command->led1 = (channel & 2 ? mode : OFF);
  command->led2 = (channel & 4 ? mode : OFF);
}

inline static void
make_full_command(struct behavior_status_led_command *command, mode mode) {
  command->led0 = mode;
  command->led1 = mode;
  command->led2 = mode;
}

K_MSGQ_DEFINE(behavior_status_led_msgq,
              sizeof(struct behavior_status_led_command), 4, 1);

/*
 * Available commands
 */
static void clear() {
  struct behavior_status_led_command command = {
      .led0 = OFF,
      .led1 = OFF,
      .led2 = OFF,
  };

  k_msgq_put(&behavior_status_led_msgq, &command, K_NO_WAIT);
}

static void display_bluetooth() {
  struct behavior_status_led_command command = {
      .led0 = OFF,
      .led1 = OFF,
      .led2 = OFF,
  };

  // TODO: The display should not be on for more than 5s
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  switch (zmk_endpoints_selected().transport) {
  case ZMK_TRANSPORT_USB:
    make_full_command(&command, FULL);
    break;
  case ZMK_TRANSPORT_BLE:
#if IS_ENABLED(CONFIG_ZMK_BLE)
    uint8_t channel = zmk_ble_active_profile_index() + 1;
    if (zmk_ble_active_profile_is_connected()) {
      make_channel_command(&command, FULL, channel);
    } else if (zmk_ble_active_profile_is_open()) {
      make_channel_command(&command, HIGH, channel);
    } else {
      make_channel_command(&command, LOW, channel);
    }
#endif // IS_ENABLED(CONFIG_ZMK_BLE)
    break;
  }
#elif IS_ENABLED(CONFIG_ZMK_SPLIT_BLE)
  if (zmk_split_bt_peripheral_is_connected()) {
    make_full_command(&command, FULL);
  } else {
    make_full_command(&command, LOW);
  }
#endif // SPLIT
  k_msgq_put(&behavior_status_led_msgq, &command, K_NO_WAIT);
}

static void display_battery() {
  struct behavior_status_led_command command = {
      .led0 = OFF,
      .led1 = OFF,
      .led2 = OFF,
  };

  uint8_t charge = zmk_battery_state_of_charge();
  for (uint8_t i = 0; charge == 0 && i < 10; ++i) {
    charge = zmk_battery_state_of_charge();
  }

  if (charge < 10) {
    command.led0 = LOW;
  } else if (charge < 20) {
    command.led0 = HIGH;
  } else if (charge < 30) {
    command.led0 = FULL;
  } else if (charge < 40) {
    command.led1 = LOW;
    command.led0 = FULL;
  } else if (charge < 50) {
    command.led1 = HIGH;
    command.led0 = FULL;
  } else if (charge < 60) {
    command.led1 = FULL;
    command.led0 = FULL;
  } else if (charge < 70) {
    command.led2 = LOW;
    command.led1 = FULL;
    command.led0 = FULL;
  } else if (charge < 80) {
    command.led2 = HIGH;
    command.led1 = FULL;
    command.led0 = FULL;
  } else if (charge < 90) {
    command.led2 = FULL;
    command.led1 = FULL;
    command.led0 = FULL;
  } else if (charge < 100) {
    command.led2 = LOW;
    command.led1 = LOW;
    command.led0 = LOW;
  }

  k_msgq_put(&behavior_status_led_msgq, &command, K_NO_WAIT);
}

// int behavior_status_led_ble_listener(const zmk_event_t *eh) {
//   display_bluetooth();
//   return ZMK_EV_EVENT_BUBBLE;
// }
//
// #if IS_ENABLED(CONFIG_ZMK_BLE)
// ZMK_LISTENER(behavior_status_led_ble, behavior_status_led_ble_listener);
// #if !IS_ENABLED(CONFIG_ZMK_SPLIT) ||
// IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
// ZMK_SUBSCRIPTION(behavior_status_led_ble, zmk_ble_active_profile_changed);
// #elif IS_ENABLED(CONFIG_ZMK_SPLIT_BLE)
// ZMK_SUBSCRIPTION(behavior_status_led_ble,
// zmk_split_peripheral_status_changed); #endif // SPLIT #endif //
// IS_ENABLED(CONFIG_ZMK_BLE)

/*
 * Background thread
 */
extern void behavior_status_led_thread_main(void *, void *, void *) {
  struct behavior_status_led_command command;
  k_timeout_t timeout = K_FOREVER;
  uint8_t step = 0;

  gpio_pin_set_dt(&led_gpios[0], 0);
  gpio_pin_set_dt(&led_gpios[1], 0);
  gpio_pin_set_dt(&led_gpios[2], 0);

  while (true) {
    k_msgq_get(&behavior_status_led_msgq, &command, timeout);

    if (command.led0 == HIGH || command.led1 == HIGH || command.led2 == HIGH) {
      timeout = K_MSEC(250);
      step += 1;
    } else if (command.led0 == LOW || command.led1 == LOW ||
               command.led2 == LOW) {
      timeout = K_MSEC(500);
      step += 2;
    } else {
      timeout = K_FOREVER;
      step = 0;
    }

    gpio_pin_set_dt(&led_gpios[0], command.led0 == FULL ||
                                       (command.led0 == HIGH && step & 1) ||
                                       (command.led0 == LOW && step & 2));
    gpio_pin_set_dt(&led_gpios[1], command.led1 == FULL ||
                                       (command.led1 == HIGH && step & 1) ||
                                       (command.led1 == LOW && step & 2));
    gpio_pin_set_dt(&led_gpios[2], command.led2 == FULL ||
                                       (command.led2 == HIGH && step & 1) ||
                                       (command.led2 == LOW && step & 2));
  }
}

K_THREAD_STACK_DEFINE(behavior_status_led_thread_stack, 128);
struct k_thread behavior_status_led_thread;
k_tid_t behavior_status_led_thread_id;

/*
 * Behavior API initialization
 */
static int behavior_status_led_init(const struct device *dev) {
  for (int i = 0; i < 3; ++i) {
    if (!device_is_ready(led_gpios[i].port)) {
      return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&led_gpios[i], GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
      return ret;
    }
  }

  behavior_status_led_thread_id = k_thread_create(
      &behavior_status_led_thread, behavior_status_led_thread_stack,
      K_THREAD_STACK_SIZEOF(behavior_status_led_thread_stack),
      behavior_status_led_thread_main, NULL, NULL, NULL,
      K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_MSEC(500));

  return 0;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
  switch (binding->param1) {
  case ST_BAT:
    display_battery();
  case ST_BLE:
    display_bluetooth();
  }
  return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
  clear();
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
