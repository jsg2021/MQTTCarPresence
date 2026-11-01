// Car presence settings.
//
// Copy this file to config.h in the same folder and fill in your values. config.h is
// git-ignored, so your Wi-Fi and MQTT credentials never end up in the repository.
//
// One sketch, several cars: uncomment exactly one profile below before each upload.

#define PROFILE_RIGHT
// #define PROFILE_LEFT

// ---- Shared settings (same for every car) -------------------------------------------------
#define WIFI_SSID             "wifissid"      // Your WiFi network name
#define WIFI_PASSWORD         "wifipassword"  // Your WiFi network password
#define OTA_PASSWORD          ""              // OTA update password, "" disables OTA
#define MQTT_SERVER           "hassio.local"  // Your MQTT server hostname or IP address
#define MQTT_USER             ""              // MQTT username, "" for no user
#define MQTT_PASSWORD         ""              // MQTT password, "" for no password
#define MQTT_DISCOVERY_PREFIX "homeassistant" // Home Assistant MQTT Discovery prefix

// ---- Per-car profiles ---------------------------------------------------------------------
// MQTT_NODE           Unique hostname; also names the Home Assistant entities
//                     (CarPresenceRight -> binary_sensor.carpresenceright).
// USE_VOLTAGE_GATE    false: ignition-switched power. true: constant power with the A0
//                     voltage gate and deep sleep. See the notes in MQTTCarPresence.ino.
// ADC_FULL_SCALE_VOLTS / VOLTS_CALIBRATION
//                     Only used with USE_VOLTAGE_GATE true.
// Any shared setting above can also be overridden inside a profile (#undef it first).

#if defined(PROFILE_RIGHT)
  #define MQTT_NODE "CarPresenceRight"
  #define USE_VOLTAGE_GATE false

#elif defined(PROFILE_LEFT)
  #define MQTT_NODE "CarPresenceLeft"
  #define USE_VOLTAGE_GATE false

#else
  #error "Pick a car profile at the top of config.h"
#endif
