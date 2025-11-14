#define DT_DRV_COMPAT zmk_behavior_status_led

#include <drivers/behavior.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

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

LOG_MODULE_REGISTER(status_led, CONFIG_ZMK_LOG_LEVEL);

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

/*
 * LED command queue
 */

inline static void
make_channel_command(struct behavior_status_led_command *command, mode mode,
                     uint8_t channel) {
  command->led0 = (channel & 1 ? mode : OFF);
  command->led1 = (channel & 2 ? mode : OFF);
  command->led2 = (channel & 4 ? mode : OFF);
  LOG_DBG("Setting channel %d command to %d [%d %d %d]", channel, mode,
          command->led0, command->led1, command->led2);
}

inline static void
make_full_command(struct behavior_status_led_command *command, mode mode) {
  command->led0 = mode;
  command->led1 = mode;
  command->led2 = mode;
  LOG_DBG("Setting full command to %d [%d %d %d]", mode, command->led0,
          command->led1, command->led2);
}

K_MSGQ_DEFINE(behavior_status_led_msgq,
              sizeof(struct behavior_status_led_command), 4, 1);

BUILD_ASSERT(CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 0 &&
                 CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER < 8,
             "Rollover count must be between 1 and 7 inclusive");
#if CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1
#define INVALID_COMMAND_IDX 128
struct behavior_status_led_command_queue_entry {
  struct zmk_behavior_binding_event event;
  struct behavior_status_led_command command;
};
static struct behavior_status_led_command_queue_entry
    command_queue[CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER];
static uint8_t rollover_count = 0;
inline static void send_command(struct zmk_behavior_binding_event *event,
                                struct behavior_status_led_command *command) {
  if (rollover_count == CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER) {
    uint8_t min_ts_idx = 0;
    int64_t min_ts = INT64_MAX;

    for (uint8_t i = 0; i < rollover_count; ++i) {
      if (command_queue[i].event.timestamp < min_ts) {
        min_ts_idx = i;
        min_ts = command_queue[i].event.timestamp;
      }
    }

    // If the min was not found, we default to removing the first one
    --rollover_count;
    command_queue[min_ts_idx] = command_queue[rollover_count];
  }
  command_queue[rollover_count] =
      (struct behavior_status_led_command_queue_entry){
          .event = *event,
          .command = {.led0 = command->led0,
                      .led1 = command->led1,
                      .led2 = command->led2}};
  ++rollover_count;
  LOG_DBG("Sending command [%d %d %d]", command->led0, command->led1,
          command->led2);
  k_msgq_put(&behavior_status_led_msgq, command, K_NO_WAIT);
}

inline static void clear(struct zmk_behavior_binding_event *event) {
  struct behavior_status_led_command command;

  if (rollover_count <= 1) {
    LOG_DBG("No command queue");
    rollover_count = 0;
    command = (struct behavior_status_led_command){
        .led0 = OFF,
        .led1 = OFF,
        .led2 = OFF,
    };
  } else {
    uint8_t clear_idx = INVALID_COMMAND_IDX;
    uint8_t max_ts_idx = INVALID_COMMAND_IDX;
    int64_t max_ts = INT64_MIN;

    for (uint8_t i = 0; i < rollover_count; ++i) {
      if (command_queue[i].event.layer == event->layer &&
          command_queue[i].event.position == event->position &&
          command_queue[i].event.source == event->source) {
        clear_idx = i;
      } else if (command_queue[i].event.timestamp > max_ts) {
        if (clear_idx >= 8) {
          clear_idx = max_ts_idx + 8;
        }
        max_ts_idx = i;
        max_ts = command_queue[i].event.timestamp;
      }
    }

    if (max_ts_idx == INVALID_COMMAND_IDX) {
      // Theoretically impossible
      LOG_DBG("No maximum timestamp. Clearing the queue");
      rollover_count = 0;
      command = (struct behavior_status_led_command){
          .led0 = OFF,
          .led1 = OFF,
          .led2 = OFF,
      };
    } else {
      if (clear_idx < 8) {
        LOG_DBG("Found command and next command in line");
        // Happy path: we found the command and a max
        command_queue[clear_idx] = command_queue[rollover_count - 1];
        command = command_queue[max_ts_idx].command;
      } else {
        // Did not find the command, delete the latest one
        command_queue[max_ts_idx] = command_queue[rollover_count - 1];

        // If we have a second largest, send that, else, clear
        if (clear_idx < 16) {
          LOG_DBG("Did not find the command. Deleting the max timestamp and "
                  "sending second largest");
          command = command_queue[clear_idx - 8].command;
        } else {
          LOG_DBG("Did not find the command. Deleting the max timestamp and "
                  "clearing");
          command = (struct behavior_status_led_command){
              .led0 = OFF,
              .led1 = OFF,
              .led2 = OFF,
          };
        }
      }
      --rollover_count;
    }
  }

  LOG_DBG("Sending command [%d %d %d]", command.led0, command.led1,
          command.led2);
  k_msgq_put(&behavior_status_led_msgq, &command, K_NO_WAIT);
}
#else
inline static void send_command(struct behavior_status_led_command *command) {
  LOG_DBG("Sending command [%d %d %d]", command->led0, command->led1,
          command->led2);
  k_msgq_put(&behavior_status_led_msgq, command, K_NO_WAIT);
}

inline static void clear() {
  struct behavior_status_led_command command = {
      .led0 = OFF,
      .led1 = OFF,
      .led2 = OFF,
  };
  LOG_DBG("Sending command [%d %d %d]", command.led0, command.led1,
          command.led2);
  k_msgq_put(&behavior_status_led_msgq, &command, K_NO_WAIT);
}
#endif // CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1

/*
 * Available commands
 */
#if CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1
static void display_bluetooth(struct zmk_behavior_binding_event *event) {
#else
static void display_bluetooth() {
#endif // CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1
  LOG_DBG("Enter");
  struct behavior_status_led_command command = {
      .led0 = OFF,
      .led1 = OFF,
      .led2 = OFF,
  };

  // TODO: The display should not be on for more than 5s
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  switch (zmk_endpoints_selected().transport) {
  case ZMK_TRANSPORT_USB:
    LOG_INF("USB connected");
    make_full_command(&command, FULL);
    break;
  case ZMK_TRANSPORT_BLE:
#if IS_ENABLED(CONFIG_ZMK_BLE)
    uint8_t channel = zmk_ble_active_profile_index() + 1;
    if (zmk_ble_active_profile_is_connected()) {
      LOG_INF("BLE %d connected", channel);
      make_channel_command(&command, FULL, channel);
    } else if (zmk_ble_active_profile_is_open()) {
      LOG_INF("BLE %d advertising", channel);
      make_channel_command(&command, HIGH, channel);
    } else {
      LOG_INF("BLE %d disconnected", channel);
      make_channel_command(&command, LOW, channel);
    }
#endif // IS_ENABLED(CONFIG_ZMK_BLE)
    break;
  }
#elif IS_ENABLED(CONFIG_ZMK_SPLIT_BLE)
  if (zmk_split_bt_peripheral_is_connected()) {
    LOG_INF("Split BLE connected");
    make_full_command(&command, FULL);
  } else {
    LOG_INF("Split BLE disconnected");
    make_full_command(&command, LOW);
  }
#endif // SPLIT
#if CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1
  send_command(event, &command);
#else
  send_command(&command);
#endif // CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1
}

#if CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1
static void display_battery(struct zmk_behavior_binding_event *event) {
#else
static void display_battery() {
#endif // CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1
  LOG_DBG("Enter");
  struct behavior_status_led_command command = {
      .led0 = OFF,
      .led1 = OFF,
      .led2 = OFF,
  };

  uint8_t charge = zmk_battery_state_of_charge();
  if (charge == 0) {
    LOG_DBG("Will retry charge. Clearing the LEDs");
    k_msgq_put(&behavior_status_led_msgq, &command, K_NO_WAIT);
    for (uint8_t i = 0; charge == 0 && i < 10; ++i) {
      k_sleep(K_MSEC(100));
      LOG_DBG("Retrying battery charge: %d", i);
      charge = zmk_battery_state_of_charge();
    }
  }
  LOG_INF("Got charge %d", charge);

  if (charge == 0) {
    command.led2 = LOW;
    command.led1 = LOW;
    command.led0 = LOW;
  } else if (charge < 10) {
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
  } else {
    command.led2 = HIGH;
    command.led1 = HIGH;
    command.led0 = HIGH;
  }
#if CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1
  send_command(event, &command);
#else
  send_command(&command);
#endif // CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1
}

/*
 * Background thread
 */
extern void behavior_status_led_thread_main(void *, void *, void *) {
  LOG_INF("Starting thread");
  struct behavior_status_led_command command;
  k_timeout_t timeout = K_FOREVER;
  uint8_t step = 0;

  gpio_pin_set_dt(&led_gpios[0], 0);
  gpio_pin_set_dt(&led_gpios[1], 0);
  gpio_pin_set_dt(&led_gpios[2], 0);

  while (true) {
    int status = k_msgq_get(&behavior_status_led_msgq, &command, timeout);
    LOG_DBG("Polled for messages. status: %d, command: [%d %d %d], step: %d",
            status, command.led0, command.led1, command.led2, step);

    if (command.led0 == HIGH || command.led1 == HIGH || command.led2 == HIGH) {
      timeout = K_MSEC(200);
      step += 1;
    } else if (command.led0 == LOW || command.led1 == LOW ||
               command.led2 == LOW) {
      timeout = K_MSEC(600);
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

K_THREAD_STACK_DEFINE(behavior_status_led_thread_stack, 512);
struct k_thread behavior_status_led_thread;
k_tid_t behavior_status_led_thread_id;

/*
 * Behavior API initialization
 */
static int behavior_status_led_init(const struct device *dev) {
  LOG_INF("Initializing behavior");

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
  LOG_DBG("Press with %d", binding->param1);
#if CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1
  switch (binding->param1) {
  case ST_BAT:
    display_battery(&event);
    break;
  case ST_BLE:
    display_bluetooth(&event);
    break;
  }
#else
  switch (binding->param1) {
  case ST_BAT:
    display_battery();
    break;
  case ST_BLE:
    display_bluetooth();
    break;
  }
#endif // CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1
  return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
  LOG_DBG("Release");
#if CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1
  clear(&event);
#else
  clear();
#endif // CONFIG_ZMK_BEHAVIOR_STATUS_LED_ROLLOVER > 1
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
