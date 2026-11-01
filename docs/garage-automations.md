# Garage automations (Home Assistant)

A backup and explanation of the Home Assistant automations that drive two garage doors from the car presence boards. Written for Home Assistant 2026.10.

Identifying details are replaced with placeholders. To restore an automation, swap them back for your own values:

| Placeholder | Meaning |
|---|---|
| `cover.left_garage_door`, `cover.right_garage_door` | The garage door entities (ratgdo openers here) |
| `sensor.driver_phone_ssid` | Wi-Fi network sensor from the right car's driver's phone (Home Assistant companion app) |
| `HOME_SSID` | Home Wi-Fi network name |

`binary_sensor.carpresenceleft` and `binary_sensor.carpresenceright` come from this repository's `PROFILE_LEFT` and `PROFILE_RIGHT` builds.

## Overview

| Automation | Trigger | Conditions | Action |
|---|---|---|---|
| Auto Open Left Garage | Left car presence turns `on` | Presence had been in its previous state over 4 minutes | Open left door |
| Auto Open Right Garage | Right car presence turns `on` | Same 4-minute check; time 06:00–midnight; the driver's phone is not on `HOME_SSID` | Open right door |
| Auto Close Left Garage | Left car presence turns `off`, or left door open 15 minutes | None | Close left door |
| Auto Close Right Garage | Right car presence turns `off`, or right door open 15 minutes | None | Close right door |

The automations target the doors by entity ID, not device ID. If an opener is replaced, give the new door entity the old entity ID (Settings → Entities → the entity → Entity ID) and the automations keep working.

## How this fits the firmware

| Situation | Presence sensor | Door |
|---|---|---|
| Car started in the garage (departure) | Stays `off` (departure detection) | Not opened. Open it yourself. Closes after being open 15 minutes. |
| Driving away | Stays `off` | No change from the car. The 15-minute rule closes it if it was left open. |
| Arriving home (out of range for over 2 min, sensor off for over 4 min) | `on` | Opens. Right door: only 06:00–midnight and when the driver's phone isn't already on home Wi-Fi. |
| Car turned off in the garage | `off` within about 20 s (MQTT last will) | Closes right away. |
| Door left open with no car event | — | Closes after 15 minutes. |

## Things worth knowing

- **Close runs even when the door is already closed.** Turning a car off with the door shut sends a close command anyway. The opener ignores it, and no logbook entry is written.
- **Opening after a Home Assistant or broker restart.** The 4-minute check measures the time since the sensor's last change. If the sensor goes `unavailable` → `on` shortly after a restart, the check fails and the door doesn't open.
- **Possible race on the right door.** If the phone joins home Wi-Fi before the car's board connects, the open is skipped.
- **No overnight opens on the right door.** Arrivals between midnight and 06:00 don't open it. The left door has no time window.

## Logbook reasons

Before each door command, the automation writes a `logbook.log` entry attached to the door. It only writes when the command will move the door: "opening" needs the door closed, "closing" needs it open. The close triggers have IDs (`car_off`, `open_15m`) so the message can say which one fired.

- `opening: CarPresenceRight arrived home (away 47 min)`
- `closing: CarPresenceRight went offline (car turned off)`
- `closing: left open for 15 minutes`

Opens skipped by a condition stop before any action, so they leave no logbook entry. Check the automation's traces for those.

To see the history, add a Logbook card to a dashboard:

```yaml
type: logbook
title: Garage door history
hours_to_show: 168
entities:
  - cover.left_garage_door
  - cover.right_garage_door
  - binary_sensor.carpresenceleft
  - binary_sensor.carpresenceright
```

## Automation YAML

Paste each into a new automation's YAML editor (⋮ → Edit in YAML) after swapping in your own values.

### Auto Open Left Garage

```yaml
alias: Auto Open Left Garage
description: Opens the door when the car's presence sensor turns on after being off for over 4 minutes. Logs the reason to the logbook.
triggers:
- entity_id:
  - binary_sensor.carpresenceleft
  to: 'on'
  trigger: state
conditions:
- condition: template
  value_template: |
    {% set minutes_in_previous_state = 4 %}

    {{ (trigger.to_state.last_changed|as_timestamp -
      trigger.from_state.last_changed|as_timestamp) / 60 >
      minutes_in_previous_state }}
actions:
- if:
  - condition: state
    entity_id: cover.left_garage_door
    state: closed
  then:
  - action: logbook.log
    data:
      name: "{{ state_attr('cover.left_garage_door', 'friendly_name') }}"
      message: 'opening: CarPresenceLeft arrived home (away {{ ((as_timestamp(trigger.to_state.last_changed) - as_timestamp(trigger.from_state.last_changed)) / 60) | round(0) | int }} min)'
      entity_id: cover.left_garage_door
- metadata: {}
  data: {}
  action: cover.open_cover
  target:
    entity_id: cover.left_garage_door
mode: single
```

### Auto Open Right Garage

```yaml
alias: Auto Open Right Garage
description: Opens the door when the car's presence sensor turns on after being off for over 4 minutes. Logs the reason to the logbook.
triggers:
- entity_id:
  - binary_sensor.carpresenceright
  to: 'on'
  trigger: state
conditions:
- condition: template
  value_template: |
    {% set minutes_in_previous_state = 4 %}

    {{ (trigger.to_state.last_changed|as_timestamp -
      trigger.from_state.last_changed|as_timestamp) / 60 >
      minutes_in_previous_state }}
- condition: time
  after: '06:00:00'
  before: '23:59:59'
  weekday:
  - sat
  - fri
  - thu
  - wed
  - tue
  - mon
  - sun
- alias: Skip if the driver's phone is on home Wi-Fi
  condition: not
  conditions:
  - condition: state
    entity_id: sensor.driver_phone_ssid
    state: HOME_SSID
actions:
- if:
  - condition: state
    entity_id: cover.right_garage_door
    state: closed
  then:
  - action: logbook.log
    data:
      name: "{{ state_attr('cover.right_garage_door', 'friendly_name') }}"
      message: 'opening: CarPresenceRight arrived home (away {{ ((as_timestamp(trigger.to_state.last_changed) - as_timestamp(trigger.from_state.last_changed)) / 60) | round(0) | int }} min)'
      entity_id: cover.right_garage_door
- action: cover.open_cover
  metadata: {}
  data: {}
  target:
    entity_id: cover.right_garage_door
mode: single
```

### Auto Close Left Garage

```yaml
alias: Auto Close Left Garage
description: Closes the door when the car's presence sensor turns off (car switched off) or the door has been open 15 minutes. Logs the reason to the logbook.
triggers:
- entity_id:
  - binary_sensor.carpresenceleft
  to: 'off'
  for:
    hours: 0
    minutes: 0
    seconds: 0
  trigger: state
  id: car_off
- trigger: state
  entity_id:
  - cover.left_garage_door
  to: open
  for:
    minutes: 15
  id: open_15m
conditions: []
actions:
- if:
  - condition: state
    entity_id: cover.left_garage_door
    state: open
  then:
  - action: logbook.log
    data:
      name: "{{ state_attr('cover.left_garage_door', 'friendly_name') }}"
      message: "{% if trigger.id == 'car_off' %}closing: CarPresenceLeft went offline (car turned off){% else %}closing: left open for 15 minutes{% endif %}"
      entity_id: cover.left_garage_door
- metadata: {}
  data: {}
  action: cover.close_cover
  target:
    entity_id: cover.left_garage_door
mode: single
```

### Auto Close Right Garage

```yaml
alias: Auto Close Right Garage
description: Closes the door when the car's presence sensor turns off (car switched off) or the door has been open 15 minutes. Logs the reason to the logbook.
triggers:
- entity_id:
  - binary_sensor.carpresenceright
  to: 'off'
  trigger: state
  id: car_off
- trigger: state
  entity_id:
  - cover.right_garage_door
  to: open
  for:
    minutes: 15
  id: open_15m
conditions: []
actions:
- if:
  - condition: state
    entity_id: cover.right_garage_door
    state: open
  then:
  - action: logbook.log
    data:
      name: "{{ state_attr('cover.right_garage_door', 'friendly_name') }}"
      message: "{% if trigger.id == 'car_off' %}closing: CarPresenceRight went offline (car turned off){% else %}closing: left open for 15 minutes{% endif %}"
      entity_id: cover.right_garage_door
- metadata: {}
  data: {}
  action: cover.close_cover
  target:
    entity_id: cover.right_garage_door
mode: single
```
