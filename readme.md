# MQTTCarPresense

> **This is a fork of [aderusha/MQTTCarPresence](https://github.com/aderusha/MQTTCarPresence).** It keeps the original hardware and Home Assistant setup and adds:
>
> * **Settings outside the source.** Wi-Fi and MQTT credentials live in a git-ignored `config.h`, with per-car profiles so one sketch can be flashed to several cars. See [Configuration](#configuration).
> * **Support for cars with no ignition-switched fuse.** An optional voltage gate reads the car's 12V on A0, so the board only goes online while the engine is running and deep sleeps otherwise. See [Power options](#power-options).
> * **Departure detection.** Starting the car while it's parked at home no longer reports presence, so it doesn't trigger a garage-open on the way out. See [Departure detection](#departure-detection).
> * **Long-term stability fixes.** Non-blocking Wi-Fi, MQTT reconnect with backoff, rollover-safe timers, no `String` churn in the main loop and a low-memory restart as a last resort.

This project utilizes a [WeMos D1 mini Pro](https://wiki.wemos.cc/products:d1:d1_mini_pro), u.FL to SMA antenna whip, and an external antenna in a 3D-printed enclosure to indicate presense when powered on and attached to MQTT.

![MQTTCarPresense](https://github.com/aderusha/MQTTCarPresence/blob/master/Images/MQTTCarPresence.jpg?raw=true)

[The included Arduino code](/MQTTCarPresence) is setup to work with [Home Assistant MQTT Discovery](https://home-assistant.io/docs/mqtt/discovery/) to automatically create a `binary_sensor` device which will indicate the connection status of the device.  The device should be powered from your car's ignition so it's only on when the car is running.  When the device powers on, it will attempt to connect to the defined MQTT broker, publishes a discovery message, and configures a Last Will and Testament.  When the device is powered off (or you drive out of WiFi range), the Last Will and Testament is sent by your broker to update subscribed automation system(s).

[The included Home Assistant blueprint automation](MQTTCarPresence.yaml) example utilizes this information to open or close a connected garage door. Just copy the file url into the blueprint import menu and create an automation using the blueprint!

[![Open your Home Assistant instance and show the blueprint import dialog with a specific blueprint pre-filled.](https://my.home-assistant.io/badges/blueprint_import.svg)](https://my.home-assistant.io/redirect/blueprint_import/?blueprint_url=https%3A%2F%2Fraw.githubusercontent.com%2Faderusha%2FMQTTCarPresence%2Fmaster%2FMQTTCarPresence.yaml)

## Workflow - User perspective

* Car is in your garage and the garage door is closed
* You get in the car and turn it on, the garage door opens
* You drive away, the garage door closes a minute later
* When you arrive back home, the garage door opens as you approach
* When you turn off your car, the garage door closes about a minute later

## Detailed workflow

* Car is powered off, garage door is closed
* Car turns on, device publishes connection message to MQTT
* Home automation platform sees that the garage door is closed and the device is connected, then sends command to open garage
* You drive away when ready
* Device eventually drives out of range, dropping connection with the broker
* MQTT broker sends disconnection message configured as the LWT
* Home automation platform sees the garage door is open and the device has disconnected, then sends command to close garage
* You go on about your day, then return home.
* Device connects to WiFi, then MQTT and publishes connection message
* Home automation platform sees that the garage door is closed and the device is connected, then sends command to open garage
* You park your car and turn it off, dropping connection with the broker
* MQTT broker sends disconnection message configured as the LWT
* Home automation platform sees the garage door is open and the device has disconnected, then sends command to close garage

## Bill of Materials

* [WeMos D1 mini Pro](https://wiki.wemos.cc/products:d1:d1_mini_pro)
* u.FL to SMA antenna whip
* External 2.4GHz WiFi antenna
* M3x8mm socket head cap screw (or similar)
* M3x4x6 threaded insert (or similar)

## Arduino programming

The first order of business will be to setup the Arduino IDE to flash the provided Arduino sketch to the ESP8266.  [Download the IDE for your platform](https://www.arduino.cc/en/Main/Software) and [follow these instructions to add support for the ESP8266 platform](https://github.com/esp8266/Arduino#installing-with-boards-manager).

Next you will need to add the PubSubClient library for MQTT.  [Follow this guide for the general process](https://www.arduino.cc/en/Guide/Libraries) and add the 'PubSubClient' from the Library Manager.

Select your board under Tools → Board (for example **LOLIN(WEMOS) D1 R2 & mini**, or **NodeMCU 1.0 (ESP-12E Module)**), set up `config.h` as described below, then upload and monitor the sensor in Home Assistant.

## Configuration

Settings are kept out of the sketch so your credentials never get committed.

1. Copy `MQTTCarPresence/config.example.h` to `MQTTCarPresence/config.h`. `config.h` is listed in `.gitignore`.
2. Fill in the shared settings: Wi-Fi network and password, MQTT broker, MQTT user and password, and optionally an OTA password.
3. Pick the car you're flashing by uncommenting exactly one profile at the top of the file:

   ```c
   #define PROFILE_RIGHT
   // #define PROFILE_LEFT
   ```

4. Upload. The Arduino IDE shows `config.h` as a tab next to the sketch.

If `config.h` is missing, the build stops with an error telling you to create it.

Each profile sets:

| Setting | Meaning |
|---|---|
| `MQTT_NODE` | Unique device name. It also names the Home Assistant entities, e.g. `CarPresenceRight` → `binary_sensor.carpresenceright`. |
| `USE_VOLTAGE_GATE` | `false` for ignition-switched power, `true` for constant power with the A0 voltage gate. See [Power options](#power-options). |
| `ADC_FULL_SCALE_VOLTS` | Voltage gate only: full-scale voltage of your A0 divider. `18.2f` for a 1.5MΩ series resistor, `16.0f` for a 0–25V voltage sensor module. |
| `VOLTS_CALIBRATION` | Voltage gate only: multimeter reading ÷ reported voltage. Defaults to `1.0f`. |

To add another car, copy a profile block, give it a new `PROFILE_...` name and a unique `MQTT_NODE`.

> ## WARNING:
> 
> If you will be deploying more than one of these devices you **must** give each profile a unique `MQTT_NODE`.  Failure to do so will result in a cascading series of MQTT connections/disconnections as your devices compete for access to your broker.

## Power options

### Ignition-switched power (`USE_VOLTAGE_GATE false`)

This is the original design: the board only has power while the car is on. No extra wiring is needed.

Make sure the source really is switched. Some modern cars have no ignition-switched fuses in the cabin, and some USB ports wake when a door opens. To test one, turn the car off, lock it, open a door a few minutes later and watch the sensor in Home Assistant; it should stay off.

### Constant power with the voltage gate (`USE_VOLTAGE_GATE true`)

Use this when only always-on power is available. The board reads the car's 12V on A0 and only turns Wi-Fi on while the alternator is charging (above about 13.1V). After about 3 minutes below 12.9V it publishes OFF and deep sleeps, waking every 20 seconds to check again. Deep sleep keeps the board's draw tiny; the 12V-to-USB adapter's own idle draw is usually what matters.

Wiring, using a resistor:

```
12V (same feed as the USB adapter) ──[ 1.5MΩ ]──┬── A0
                                                 │
                                              [0.1µF]
                                                 │
                                                GND
D0 ──── RST   (required for wake from deep sleep; remove it if USB uploads fail)
```

Put the resistor at the 12V end so a chafed wire can't short anything. Ground comes through the USB adapter. A solderless alternative is a "0–25V voltage sensor module": screw terminals to 12V and ground, its S pin to A0 and – pin to GND, then set `ADC_FULL_SCALE_VOLTS` to `16.0f`.

After installing, compare the `CarPresence...-voltage` sensor in Home Assistant to a multimeter at the fuse with the engine running, and set `VOLTS_CALIBRATION` if they differ.

Full build notes, wiring for both options, parts lists and a test checklist are in [docs/voltage-gate.md](docs/voltage-gate.md) and [docs/voltage-gate-parts.xlsx](docs/voltage-gate-parts.xlsx).

## Departure detection

Because the board powers up whenever the car starts, starting a car that's parked at home would otherwise look like an arrival. In this fork:

* If the car had been parked and the board connects to home Wi-Fi within 90 seconds of starting, it's treated as **leaving**, and presence stays OFF.
* Presence reports ON only on a later connection, after Wi-Fi has been out of range for at least 2 minutes. In other words, you drove away and came back.
* Turning the car off reports OFF, through the MQTT Last Will within about 20 seconds, or straight away when the voltage gate puts the board to sleep.

This changes the workflows above. Starting the car in the garage **does not open the door**, so open it yourself on the way out. Because presence never goes ON during a departure, the blueprint won't close the door behind you either. Arriving home, opening the door, and closing it after you turn the car off all work as before.

The timings (`departureWindowMs`, `departureClearMs`, `parkedSleepWakes`) are near the top of the sketch.

## Over-the-air updates

OTA is off unless `OTA_PASSWORD` is set in `config.h`. Set a password and flash once over USB. After that, with the car on and connected to Wi-Fi, the board appears in the Arduino IDE under Tools → Port as a network port named after `MQTT_NODE`. Your computer must be on the same network (or VLAN) as the board.

## WeMos D1 Mini Pro Antenna

In order to utilize an external antenna on the WeMos D1 Mini Pro, you will need to move a 0 ohm 0603 SMT resistor.  See [this blog post](http://raspi.tv/2017/how-to-use-external-antenna-on-wemos-d1-mini-pro-surface-mount-rework-video) along with [an accompanying video](https://www.youtube.com/watch?v=3lHoG1mu7hY&t=72) on how to make that happen.  A hot-air station will make this easier, but I was able to do this with a normal pencil iron.

## Tasmota Firmware Variant

If you are more familiar to Tasmota flashing and usage rather than Arduino Programming and building, this is another variant that you could use.  It will replace the **Arduino Programming** section above, but follows the guidelines of just about everything else.  The blueprint that is attached to this project named MQTT_Tasmota_CarPresence.yaml can be used as an alternate along with flashing the Di-Mini with Tasmota to accomplish the same thing as the original.  

This Tasmota Variant is set-up to work with [Home Assistant Tasmota Integration](https://www.home-assistant.io/integrations/tasmota/) to automatically create a `switch` which will indicate the connection status of the Car Presence Device.  It should show up as a switch with a name that matches the FriendlyName set-up in the Tasmota Web Console.

####    Follow these steps to flash the D1 Mini if using Tasmota: ####

* Flash the D1 Mini to the latest Tasmota version.
* Set up your WIFI, MQTT, Hostname, Friendly name, and all the normal stuff.  Details on the Tasmota homepage.
* Set the Device module to type `generic(18)`.
* Under configure module, 
    * set GPIO2 as LEDi 1
    * set GPIO4 as Button 1
    * set GPIO5 as Relay 1
* On the console, paste this to set-up the mcu functions as required for proper operation (Check Tasmota website for details on what these are):  
    * ```Backlog  SO19 0; SO56 1; SO63 1; SO65 1; WifiConfig 5```
* Go to Configure Other and be sure to set a Web Admin Password in case it goes into AP mode when you are away from Home.
* Set-up the blueprint `MQTT_Tasmota_CarPresence.yaml` similar to the instructions for the other blueprint above.

[![Open your Home Assistant instance and show the blueprint import dialog with a specific blueprint pre-filled.](https://my.home-assistant.io/badges/blueprint_import.svg)](https://my.home-assistant.io/redirect/blueprint_import/?blueprint_url=https%3A%2F%2Fgithub.com%2Faderusha%2FMQTTCarPresence%2Fblob%2Fmaster%2FMQTT_Tasmota_CarPresence.yaml)

## Buy me a coffee

*From the original author, [aderusha](https://github.com/aderusha):*

[![Buy me a coffee](https://www.buymeacoffee.com/assets/img/custom_images/black_img.png)](https://www.buymeacoffee.com/gW5rPpsKR)

This project is powered by coffee.  [I might get a little weird about it at times](https://github.com/aderusha/RoastLearner), but it's not much of a stretch to suggest that coffee both powers and consumes a fair portion of my mental energy.  [Hook me up if you think HASP is cool](https://www.buymeacoffee.com/gW5rPpsKR).  Thanks!
