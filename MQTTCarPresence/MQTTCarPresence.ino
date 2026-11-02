////////////////////////////////////////////////////////////////////////////////////////////////////
// Settings live in config.h, which is git-ignored so credentials are never committed.
// Copy config.example.h to config.h, fill in your values and pick a car profile.
#if __has_include("config.h")
#include "config.h"
#else
#error "config.h not found: copy config.example.h to config.h and fill in your settings"
#endif

#ifndef USE_VOLTAGE_GATE
#define USE_VOLTAGE_GATE false
#endif
#ifndef ADC_FULL_SCALE_VOLTS
#define ADC_FULL_SCALE_VOLTS 18.2f
#endif
#ifndef VOLTS_CALIBRATION
#define VOLTS_CALIBRATION 1.0f
#endif
#ifndef WIFI_STATIC_IP
#define WIFI_STATIC_IP "" // "" uses DHCP
#endif
#ifndef WIFI_GATEWAY
#define WIFI_GATEWAY ""
#endif
#ifndef WIFI_SUBNET
#define WIFI_SUBNET "255.255.255.0"
#endif
#ifndef WIFI_DNS
#define WIFI_DNS "" // defaults to the gateway
#endif

const char *wifiSSID = WIFI_SSID;
const char *wifiPassword = WIFI_PASSWORD;
const char *otaPassword = OTA_PASSWORD;
const char *mqttServer = MQTT_SERVER;
const char *mqttUser = MQTT_USER;
const char *mqttPassword = MQTT_PASSWORD;
const String mqttNode = MQTT_NODE;
const String mqttDiscoveryPrefix = MQTT_DISCOVERY_PREFIX;
const char *wifiStaticIp = WIFI_STATIC_IP;
const char *wifiGateway = WIFI_GATEWAY;
const char *wifiSubnet = WIFI_SUBNET;
const char *wifiDns = WIFI_DNS;
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fast Wi-Fi connect
//
// A normal connect scans every channel for the network, which takes a second or two. The board saves
// the access point (BSSID) and channel it last joined in flash and goes straight to it on the next boot.
// If that access point doesn't answer within fastConnectTimeoutMs (a different mesh node is closer, or
// the router changed channel), it falls back to a full scan. Flash is only written when the access
// point or channel changes. Set WIFI_STATIC_IP in config.h to skip DHCP as well, and use an IP address
// for MQTT_SERVER to skip the DNS lookup.
const unsigned long fastConnectTimeoutMs = 3000;
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
// Power source
//
// false: Ignition-switched supply (an accessory circuit that's only live while the car runs).
//   - No A0 wiring and no D0-RST jumper needed; the board runs whenever the adapter has power.
//   - Every ignition-on is a fresh boot. If home Wi-Fi connects within departureWindowMs, the car
//     is leaving and presence stays OFF. A later connection, after being out of range for at
//     least departureClearMs, counts as an arrival and publishes ON.
//   - When the car shuts off, power is cut and the MQTT last will sets presence OFF within ~20s.
//   - No voltage sensor is published (A0 is unused).
//   - Shut-off test after install: turn the car off, lock it, open a door a few minutes later and
//     watch the sensor in Home Assistant. It must stay OFF. If it comes back ON, the adapter is
//     not truly ignition-switched; use a constant fuse with useVoltageGate = true instead.
//
// true: Constant power (a fuse tap in the cabin fuse box).
//   - Needs the car's 12V on A0 and D0 jumpered to RST (see wiring below).
//   - Only turns Wi-Fi on while the alternator is charging, deep sleeps otherwise.
//   - Publishes a CarPresenceRight-voltage sensor for tuning and calibration.
//   - Set ADC_FULL_SCALE_VOLTS in config.h to match the divider: 18.2f for a 1.5M series
//     resistor, 16.0f for a 0-25V voltage sensor module.
const bool useVoltageGate = USE_VOLTAGE_GATE; // set per car in config.h

////////////////////////////////////////////////////////////////////////////////////////////////////
// Engine detection (voltage gate) and deep sleep -- only used when useVoltageGate is true
//
// Wiring:  12V fuse tap --[ 1.5M ]--+-- A0
//                                   |
//                                [0.1uF]
//                                   |
//                                  GND
//          D0 jumpered to RST (required for deep-sleep wake; pull it if USB flashing fails)
//
// The D1 Mini's onboard 220k/100k divider plus the 1.5M gives ~18.2V full scale on A0.
// After install, compare the published voltage to a multimeter at the fuse and set
// VOLTS_CALIBRATION = multimeter / reported (in config.h).
const float adcFullScaleVolts = ADC_FULL_SCALE_VOLTS; // set per car in config.h
float voltsCalibration = VOLTS_CALIBRATION; // set per car in config.h

const float engineOnVolts = 13.1f;               // boot only proceeds above this (alternator charging)
const float engineOffVolts = 12.9f;              // below this counts as "engine off"
const unsigned long engineOffHoldMs = 180000;    // must stay below engineOffVolts this long before sleeping
                                                 // (long enough to ride out smart-charging dips)
const uint32_t sleepIntervalUs = 20000000UL;     // engine-off check interval while asleep (20s)
const unsigned long voltsSampleMs = 1000;        // don't read A0 faster than this while Wi-Fi is on

// Departure detection: if the car had been parked (asleep at least parkedSleepWakes in a row) and Wi-Fi
// connects within departureWindowMs of engine start, the car started at home and is leaving, so presence
// stays OFF. Presence goes ON only on a later connection, after Wi-Fi has been out of range for at least
// departureClearMs (the car actually left and came back). A short sleep from a smart-charging voltage
// dip while driving doesn't count as parked, so pulling in after one still reads as an arrival.
const unsigned long departureWindowMs = 90000;
const unsigned long departureClearMs = 120000;
const uint32_t parkedSleepWakes = 30;            // 30 x 20s = 10 minutes asleep

// Drive-away detection: presence stays OFF while departing, so nothing else tells Home Assistant the car
// has gone. During a departure session the board watches a smoothed Wi-Fi signal. Once it has been strong
// (started in the garage) and then fades below departFarRssi, the car is pulling away and the -departed
// sensor turns ON while the link still works. A car that falls asleep in the garage just loses power, with
// no fade first, so it never reports departed.
const float departNearRssi = -70;                // smoothed signal must reach this first (in or near the garage)
const float departFarRssi = -80;                 // then fall below this
const float rssiSmoothing = 0.3f;                // weight of each new sample; rides out single-sample blips
const unsigned long rssiSampleMs = 500;
const unsigned long departedReportWindowMs = 10000; // drop a departed report that couldn't be sent by then
////////////////////////////////////////////////////////////////////////////////////////////////////

// Home Assistant MQTT Discovery, see https://home-assistant.io/docs/mqtt/discovery/
// Topics and names for the binary sensor are unchanged so existing automations keep working.
const String mqttDiscoBinaryStateTopic = mqttDiscoveryPrefix + "/binary_sensor/" + mqttNode + "/state";
const String mqttDiscoBinaryConfigTopic = mqttDiscoveryPrefix + "/binary_sensor/" + mqttNode + "/config";
const String mqttDiscoSignalStateTopic = mqttDiscoveryPrefix + "/sensor/" + mqttNode + "-signal/state";
const String mqttDiscoSignalConfigTopic = mqttDiscoveryPrefix + "/sensor/" + mqttNode + "-signal/config";
const String mqttDiscoUptimeStateTopic = mqttDiscoveryPrefix + "/sensor/" + mqttNode + "-uptime/state";
const String mqttDiscoUptimeConfigTopic = mqttDiscoveryPrefix + "/sensor/" + mqttNode + "-uptime/config";
// New: car battery voltage, for tuning the thresholds above
const String mqttDiscoVoltsStateTopic = mqttDiscoveryPrefix + "/sensor/" + mqttNode + "-voltage/state";
const String mqttDiscoVoltsConfigTopic = mqttDiscoveryPrefix + "/sensor/" + mqttNode + "-voltage/config";
// New: turns ON for a minute when the car drives away from home
const String mqttDiscoDepartedStateTopic = mqttDiscoveryPrefix + "/binary_sensor/" + mqttNode + "-departed/state";
const String mqttDiscoDepartedConfigTopic = mqttDiscoveryPrefix + "/binary_sensor/" + mqttNode + "-departed/config";

const String mqttDiscoBinaryConfigPayload = "{\"name\": \"" + mqttNode + "\", \"device_class\": \"connectivity\", \"state_topic\": \"" + mqttDiscoBinaryStateTopic + "\"}";
const String mqttDiscoSignalConfigPayload = "{\"name\": \"" + mqttNode + "-signal\", \"state_topic\": \"" + mqttDiscoSignalStateTopic + "\", \"unit_of_measurement\": \"dBm\", \"value_template\": \"{{ value }}\"}";
const String mqttDiscoUptimeConfigPayload = "{\"name\": \"" + mqttNode + "-uptime\", \"state_topic\": \"" + mqttDiscoUptimeStateTopic + "\", \"unit_of_measurement\": \"msec\", \"value_template\": \"{{ value }}\"}";
const String mqttDiscoVoltsConfigPayload = "{\"name\": \"" + mqttNode + "-voltage\", \"state_topic\": \"" + mqttDiscoVoltsStateTopic + "\", \"unit_of_measurement\": \"V\", \"device_class\": \"voltage\", \"state_class\": \"measurement\"}";
const String mqttDiscoDepartedConfigPayload = "{\"name\": \"" + mqttNode + "-departed\", \"state_topic\": \"" + mqttDiscoDepartedStateTopic + "\", \"off_delay\": 60}";

// Reporting and LED timing
const unsigned long reportInterval = 5000;
const unsigned long twinkleInterval = 50;

#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Runtime state (all timers use "millis() - start >= interval", which is rollover-safe)
float carVolts = 0;
unsigned long lastVoltsSampleMs = 0;
unsigned long lastEngineOnMs = 0;
unsigned long reportTimer = 0;
unsigned long twinkleTimer = 0;
unsigned long lastMqttAttemptMs = 0;
unsigned long mqttRetryMs = 0;                   // backoff, grows to 60s on repeated failures
unsigned long wifiLostMs = 0;
bool wifiWasConnected = false;
bool everConnected = false;
bool departureSession = false;
bool otaStarted = false;
bool wasParked = true;
unsigned long lastRssiSampleMs = 0;
float smoothedRssi = 0;                          // 0 = no sample yet
bool sawStrongSignal = false;
bool departed = false;                           // signal faded; stop watching
bool departedReported = false;                   // published to MQTT (or dropped as too old)
unsigned long departedAtMs = 0;
unsigned long wifiBeginMs = 0;
bool fastConnecting = false;

// Last access point joined, kept in flash so it survives power loss
struct ApCache
{
  uint32_t magic;
  uint8_t channel;
  uint8_t bssid[6];
};
const uint32_t apCacheMagic = 0xAC0FFEE1;
ApCache apCache;

// Survives deep sleep (RTC user memory), reset on power loss
struct RtcState
{
  uint32_t magic;
  uint32_t offWakes; // consecutive engine-off wakes since the engine last ran
};
const uint32_t rtcMagic = 0xCA5E0001;
RtcState rtcState;

void saveRtc() { ESP.rtcUserMemoryWrite(0, (uint32_t *)&rtcState, sizeof(rtcState)); }

float readCarVolts();
void goToSleep();
bool mqttConnect();
void setupOTA();
void startWifi();
void rememberAccessPoint();
void watchSignal(unsigned long now);
const char *presenceState() { return departureSession ? "OFF" : "ON"; }

////////////////////////////////////////////////////////////////////////////////////////////////////
// System setup
void setup()
{
  // Keep the radio off until we know the engine is running. With useVoltageGate = false there is
  // nothing to wait for (the board only has power with the ignition on), so the radio comes on
  // right after this, a few milliseconds into boot.
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.begin(115200);

  ESP.rtcUserMemoryRead(0, (uint32_t *)&rtcState, sizeof(rtcState));
  if (rtcState.magic != rtcMagic)
  {
    // Fresh power-up: treat as parked
    rtcState.magic = rtcMagic;
    rtcState.offWakes = parkedSleepWakes;
  }

  carVolts = useVoltageGate ? readCarVolts() : 0;
  Serial.printf("\nBoot, car voltage %.2fV, %u engine-off wakes\n", carVolts, rtcState.offWakes);
  if (useVoltageGate && carVolts < engineOnVolts)
  {
    if (rtcState.offWakes < parkedSleepWakes)
    {
      rtcState.offWakes++;
    }
    saveRtc();
    goToSleep(); // does not return
  }

  wasParked = (rtcState.offWakes >= parkedSleepWakes);
  rtcState.offWakes = 0;
  saveRtc();

  Serial.println(useVoltageGate ? "Engine running, starting Wi-Fi" : "Ignition on, starting Wi-Fi");
  WiFi.forceSleepWake();
  delay(1);
  WiFi.hostname(mqttNode.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  startWifi(); // non-blocking; loop() watches the status

  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setBufferSize(512);  // discovery payloads exceed the default 128
  mqttClient.setSocketTimeout(5); // don't stall loop() for 15s when the broker is unreachable
  mqttClient.setKeepAlive(15);

  lastEngineOnMs = millis();
  lastVoltsSampleMs = millis();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Main execution loop
void loop()
{
  unsigned long now = millis();

  // Engine check
  if (useVoltageGate && now - lastVoltsSampleMs >= voltsSampleMs)
  {
    lastVoltsSampleMs = now;
    carVolts = readCarVolts();
    if (carVolts >= engineOffVolts)
    {
      lastEngineOnMs = now;
    }
    else if (now - lastEngineOnMs >= engineOffHoldMs)
    {
      goToSleep(); // does not return
    }
  }

  bool wifiUp = (WiFi.status() == WL_CONNECTED);

  if (!wifiUp && fastConnecting && now - wifiBeginMs >= fastConnectTimeoutMs)
  {
    Serial.println("Saved access point didn't answer, scanning");
    fastConnecting = false;
    WiFi.begin(wifiSSID, wifiPassword);
  }

  if (wifiUp && !wifiWasConnected)
  {
    Serial.printf("Wi-Fi connected in %lu ms, IP %s, channel %d, %d dBm\n", now - wifiBeginMs,
                  WiFi.localIP().toString().c_str(), WiFi.channel(), WiFi.RSSI());
    fastConnecting = false;
    rememberAccessPoint();
    if (!everConnected)
    {
      everConnected = true;
      departureSession = wasParked && (now < departureWindowMs);
      if (departureSession)
      {
        Serial.println("Connected right after engine start: departing, presence stays OFF");
      }
    }
    mqttRetryMs = 0; // connect to MQTT right away
  }
  else if (!wifiUp && wifiWasConnected)
  {
    Serial.println("Wi-Fi lost");
    wifiLostMs = now;
    digitalWrite(LED_BUILTIN, HIGH);
    // Reconnect to whichever access point is in range when the car comes back, not just the saved one
    WiFi.begin(wifiSSID, wifiPassword);
  }
  else if (!wifiUp && departureSession && (now - wifiLostMs >= departureClearMs))
  {
    Serial.println("Out of Wi-Fi range long enough: next connection counts as arrival");
    departureSession = false;
  }
  wifiWasConnected = wifiUp;

  if (wifiUp)
  {
    if (!otaStarted && otaPassword[0])
    {
      setupOTA();
      otaStarted = true;
    }

    // Watch from the moment Wi-Fi is up, so a car that's already pulling out by the time MQTT connects
    // still sees its strong in-garage signal first
    if (departureSession && !departed && now - lastRssiSampleMs >= rssiSampleMs)
    {
      lastRssiSampleMs = now;
      watchSignal(now);
    }

    if (!mqttClient.connected())
    {
      if (now - lastMqttAttemptMs >= mqttRetryMs)
      {
        lastMqttAttemptMs = now;
        if (mqttConnect())
        {
          mqttRetryMs = 2000;
        }
        else
        {
          mqttRetryMs = (mqttRetryMs < 2000) ? 2000 : min(mqttRetryMs * 2, 60000UL);
        }
      }
    }
    else
    {
      mqttClient.loop();

      if (departed && !departedReported)
      {
        // Only while it's news: a report that couldn't go out before the link dropped must not
        // turn up on a later connection, such as arriving home
        if (departureSession && now - departedAtMs <= departedReportWindowMs)
        {
          departedReported = mqttClient.publish(mqttDiscoDepartedStateTopic.c_str(), "ON");
        }
        else
        {
          Serial.println("Departed report too old, dropping it");
          departedReported = true;
        }
      }

      if (now - twinkleTimer >= twinkleInterval)
      {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        twinkleTimer = now;
      }

      if (now - reportTimer >= reportInterval)
      {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", WiFi.RSSI());
        mqttClient.publish(mqttDiscoSignalStateTopic.c_str(), buf);
        snprintf(buf, sizeof(buf), "%lu", now);
        mqttClient.publish(mqttDiscoUptimeStateTopic.c_str(), buf);
        if (useVoltageGate)
        {
          snprintf(buf, sizeof(buf), "%.2f", carVolts);
          mqttClient.publish(mqttDiscoVoltsStateTopic.c_str(), buf);
        }
        mqttClient.publish(mqttDiscoBinaryStateTopic.c_str(), presenceState());
        reportTimer = now;
      }
    }

    if (otaStarted)
    {
      ArduinoOTA.handle();
    }
  }

  // Last-resort safety net: restart cleanly instead of limping along with a starved heap
  if (ESP.getFreeHeap() < 6000)
  {
    Serial.println("Low heap, restarting");
    Serial.flush();
    ESP.restart();
  }

  delay(10); // yield to the Wi-Fi stack
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions

// Average a few A0 samples and convert to car volts
float readCarVolts()
{
  uint32_t sum = 0;
  const int samples = 8;
  for (int i = 0; i < samples; i++)
  {
    sum += analogRead(A0);
    delay(2);
  }
  return (sum / (float)samples) * adcFullScaleVolts / 1023.0f * voltsCalibration;
}

// Report OFF, shut the radio down and deep sleep. Wakes via D0->RST and reboots into setup().
void goToSleep()
{
  if (mqttClient.connected())
  {
    mqttClient.publish(mqttDiscoBinaryStateTopic.c_str(), "OFF", true);
    mqttClient.loop();
    delay(50);
    mqttClient.disconnect();
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.printf("Engine off (%.2fV), sleeping\n", carVolts);
  Serial.flush();
  ESP.deepSleep(sleepIntervalUs);
}

// Start connecting: static IP if configured, and straight to the saved access point if there is one
void startWifi()
{
  if (wifiStaticIp[0])
  {
    IPAddress ip, gateway, subnet, dns;
    if (ip.fromString(wifiStaticIp) && gateway.fromString(wifiGateway) && subnet.fromString(wifiSubnet))
    {
      if (!dns.fromString(wifiDns))
      {
        dns = gateway;
      }
      WiFi.config(ip, gateway, subnet, dns);
    }
    else
    {
      Serial.println("WIFI_STATIC_IP, WIFI_GATEWAY or WIFI_SUBNET is not a valid address, using DHCP");
    }
  }

  EEPROM.begin(sizeof(ApCache));
  EEPROM.get(0, apCache);
  wifiBeginMs = millis();
  if (apCache.magic == apCacheMagic && apCache.channel >= 1 && apCache.channel <= 14)
  {
    Serial.printf("Connecting to saved access point on channel %d\n", apCache.channel);
    fastConnecting = true;
    WiFi.begin(wifiSSID, wifiPassword, apCache.channel, apCache.bssid);
  }
  else
  {
    WiFi.begin(wifiSSID, wifiPassword);
  }
}

// Save the access point and channel we joined, only if they changed (each write wears the flash)
void rememberAccessPoint()
{
  uint8_t channel = WiFi.channel();
  const uint8_t *bssid = WiFi.BSSID();
  if (apCache.magic == apCacheMagic && apCache.channel == channel && memcmp(apCache.bssid, bssid, 6) == 0)
  {
    return;
  }
  apCache.magic = apCacheMagic;
  apCache.channel = channel;
  memcpy(apCache.bssid, bssid, 6);
  EEPROM.put(0, apCache);
  EEPROM.commit();
  Serial.printf("Saved access point %s, channel %d\n", WiFi.BSSIDstr().c_str(), channel);
}

// Departure only: smooth the signal and flag "departed" once it has been strong and then fades
void watchSignal(unsigned long now)
{
  int rssi = WiFi.RSSI();
  smoothedRssi = (smoothedRssi == 0) ? rssi : smoothedRssi + rssiSmoothing * (rssi - smoothedRssi);
  if (smoothedRssi >= departNearRssi)
  {
    sawStrongSignal = true;
  }
  else if (sawStrongSignal && smoothedRssi < departFarRssi)
  {
    Serial.printf("Signal faded to %.0f dBm at %lu ms: driving away, reporting departed\n", smoothedRssi, now);
    departed = true;
    departedAtMs = now;
  }
}

// MQTT connection and discovery. Returns true when connected.
bool mqttConnect()
{
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.printf("Attempting MQTT connection to broker: %s\n", mqttServer);
  // Last will: broker marks us OFF if we vanish (power cut, out of range)
  if (!mqttClient.connect(mqttNode.c_str(), mqttUser, mqttPassword, mqttDiscoBinaryStateTopic.c_str(), 1, true, "OFF"))
  {
    Serial.printf("MQTT connection failed, rc=%d\n", mqttClient.state());
    return false;
  }

  char buf[16];
  mqttClient.publish(mqttDiscoBinaryConfigTopic.c_str(), mqttDiscoBinaryConfigPayload.c_str(), true);
  mqttClient.publish(mqttDiscoSignalConfigTopic.c_str(), mqttDiscoSignalConfigPayload.c_str(), true);
  mqttClient.publish(mqttDiscoUptimeConfigTopic.c_str(), mqttDiscoUptimeConfigPayload.c_str(), true);
  mqttClient.publish(mqttDiscoDepartedConfigTopic.c_str(), mqttDiscoDepartedConfigPayload.c_str(), true);
  if (useVoltageGate)
  {
    mqttClient.publish(mqttDiscoVoltsConfigTopic.c_str(), mqttDiscoVoltsConfigPayload.c_str(), true);
  }

  mqttClient.publish(mqttDiscoBinaryStateTopic.c_str(), presenceState());
  snprintf(buf, sizeof(buf), "%d", WiFi.RSSI());
  mqttClient.publish(mqttDiscoSignalStateTopic.c_str(), buf);
  snprintf(buf, sizeof(buf), "%lu", millis());
  mqttClient.publish(mqttDiscoUptimeStateTopic.c_str(), buf);
  if (useVoltageGate)
  {
    snprintf(buf, sizeof(buf), "%.2f", carVolts);
    mqttClient.publish(mqttDiscoVoltsStateTopic.c_str(), buf);
  }

  reportTimer = millis();
  Serial.printf("MQTT connected at %lu ms, presence %s\n", millis(), presenceState());
  digitalWrite(LED_BUILTIN, LOW);
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// (mostly) boilerplate OTA setup from library examples
void setupOTA()
{
  ArduinoOTA.setHostname(mqttNode.c_str());
  ArduinoOTA.setPassword(otaPassword);
  ArduinoOTA.onStart([]() { Serial.println("ESP OTA:  update start"); });
  ArduinoOTA.onEnd([]() { Serial.println("ESP OTA:  update complete"); });
  ArduinoOTA.onError([](ota_error_t error) { Serial.printf("ESP OTA:  ERROR code %u\n", error); });
  ArduinoOTA.begin();
  Serial.println("ESP OTA:  Over the Air firmware update ready");
}
