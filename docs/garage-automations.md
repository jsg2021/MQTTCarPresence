# Garage automations (Home Assistant)

A backup and explanation of the Home Assistant automations that drive two garage doors from the car presence boards. Written for Home Assistant 2026.10.

Identifying details are replaced with placeholders. To restore an automation, swap them back for your own values:

| Placeholder | Meaning |
|---|---|
| `cover.left_garage_door`, `cover.right_garage_door` | The garage door entities (ratgdo openers here) |
| `sensor.driver_phone_ssid` | Wi-Fi network sensor from the right car's driver's phone (Home Assistant companion app) |
| `notify.mobile_app_driver_phone` | Companion app notify service for that phone |
| `person.driver` | That driver's person entity |
| `HOME_SSID` | Home Wi-Fi network name |

`binary_sensor.carpresenceleft` and `binary_sensor.carpresenceright` come from this repository's `PROFILE_LEFT` and `PROFILE_RIGHT` builds.

## Overview

| Automation | Trigger | Conditions | Action |
|---|---|---|---|
| Auto Open Left Garage | Left car presence turns `on` | Presence had been in its previous state over 4 minutes | Open left door |
| Auto Open Right Garage | Right car presence turns `on` | Same 4-minute check; time 06:00–midnight; the driver's phone is not on `HOME_SSID` | Open right door |
| Auto Close Left Garage | Left car presence turns `off`, or left door open 15 minutes | None | Close left door |
| Auto Close Right Garage | Right car presence turns `off`, or right door open 15 minutes | None | Car off: close right door. Open 15 minutes: alert the driver first if they're home (see below) |

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

## Keep-open alert (right door)

The right car's driver sometimes sits in the car with the door open long enough for the car to go back to sleep. When the right door has been open 15 minutes:

- **Driver not home:** the door closes, as before.
- **Driver home:** their phone gets a time-sensitive notification, "Closing in 2 minutes unless you keep it open", with a **Keep open** button.
  On iPhone the button only appears after pressing and holding the notification. A plain tap just opens the app and does nothing, so the alert text says so.
  - **No tap within 2 minutes:** the door closes.
  - **Keep open tapped:** the door stays open for 30 minutes, then the driver is asked again. This repeats until they stop tapping or the door is closed.
  - **Door closed any other way:** the alert flow stops, and the notification is cleared.

The automation runs in `restart` mode. If the car's presence turns off during the alert flow, the close-on-car-off path takes over at once.

## Logbook reasons

Before each door command, the automation writes a `logbook.log` entry attached to the door. It only writes when the command will move the door: "opening" needs the door closed, "closing" needs it open. The close triggers have IDs (`car_off`, `open_15m`) so the message can say which one fired.

- `opening: CarPresenceRight arrived home (away 47 min)`
- `closing: CarPresenceRight went offline (car turned off)`
- `closing: left open for 15 minutes`
- `alert sent to the driver: closing in 2 minutes unless kept open` (right door)
- `kept open for 30 minutes (the driver tapped Keep open)` (right door)
- `closing: left open, no response to the driver's alert` (right door)

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
description: >-
  Closes the right door when the car's presence sensor turns off (car switched off),
  or when the door has been open 15 minutes. For the 15-minute case, if the driver is home
  the driver's phone gets a "Keep open" alert first; tapping it holds the door for 30 minutes,
  then they are asked again. No answer within 2 minutes closes the door. Logs every step.
mode: restart
variables:
  door: cover.right_garage_door
  notify_tag: right-garage-auto-close
  keep_action: RIGHT_GARAGE_KEEP_OPEN
  kept: false
triggers:
  - trigger: state
    entity_id:
      - binary_sensor.carpresenceright
    to: "off"
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
  - choose:
      - conditions:
          - condition: trigger
            id: car_off
        sequence:
          - if:
              - condition: state
                entity_id: cover.right_garage_door
                state: open
            then:
              - action: logbook.log
                data:
                  name: "{{ state_attr(door, 'friendly_name') }}"
                  message: "closing: CarPresenceRight went offline (car turned off)"
                  entity_id: cover.right_garage_door
          - action: cover.close_cover
            target:
              entity_id: cover.right_garage_door
      - conditions:
          - condition: trigger
            id: open_15m
          - condition: not
            conditions:
              - condition: state
                entity_id: person.driver
                state: home
        sequence:
          - action: logbook.log
            data:
              name: "{{ state_attr(door, 'friendly_name') }}"
              message: "closing: left open for 15 minutes (the driver not home, no alert)"
              entity_id: cover.right_garage_door
          - action: cover.close_cover
            target:
              entity_id: cover.right_garage_door
      - conditions:
          - condition: trigger
            id: open_15m
        sequence:
          - repeat:
              sequence:
                - action: notify.mobile_app_driver_phone
                  data:
                    title: Right garage door
                    message: Open for a while. Closing in 2 minutes. To keep it open, press and hold this notification, then tap Keep open.
                    data:
                      tag: "{{ notify_tag }}"
                      push:
                        interruption-level: time-sensitive
                      actions:
                        - action: "{{ keep_action }}"
                          title: Keep open
                - action: logbook.log
                  data:
                    name: "{{ state_attr(door, 'friendly_name') }}"
                    message: "alert sent to the driver: closing in 2 minutes unless kept open"
                    entity_id: cover.right_garage_door
                - wait_for_trigger:
                    - trigger: event
                      event_type: mobile_app_notification_action
                      event_data:
                        action: RIGHT_GARAGE_KEEP_OPEN
                      id: keep
                    - trigger: state
                      entity_id: cover.right_garage_door
                      not_to: open
                      id: door_moved
                  timeout:
                    minutes: 2
                  continue_on_timeout: true
                - variables:
                    kept: "{{ wait.trigger is not none and wait.trigger.id == 'keep' }}"
                - if:
                    - condition: template
                      value_template: "{{ kept }}"
                  then:
                    - action: logbook.log
                      data:
                        name: "{{ state_attr(door, 'friendly_name') }}"
                        message: "kept open for 30 minutes (the driver tapped Keep open)"
                        entity_id: cover.right_garage_door
                    - wait_for_trigger:
                        - trigger: state
                          entity_id: cover.right_garage_door
                          not_to: open
                      timeout:
                        minutes: 30
                      continue_on_timeout: true
              until:
                - condition: template
                  value_template: "{{ not kept or not is_state(door, 'open') }}"
          - action: notify.mobile_app_driver_phone
            data:
              message: clear_notification
              data:
                tag: "{{ notify_tag }}"
          - if:
              - condition: state
                entity_id: cover.right_garage_door
                state: open
            then:
              - action: logbook.log
                data:
                  name: "{{ state_attr(door, 'friendly_name') }}"
                  message: "closing: left open, no response to the driver's alert"
                  entity_id: cover.right_garage_door
              - action: cover.close_cover
                target:
                  entity_id: cover.right_garage_door
```
