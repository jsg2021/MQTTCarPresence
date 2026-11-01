# Voltage gate build (paused)

Notes for resuming the constant-power install. The parts lists for both options are in [voltage-gate-parts.xlsx](voltage-gate-parts.xlsx), with a checklist column.

## Status

**Paused.** The car is being tried on an ignition-switched power source first, with `USE_VOLTAGE_GATE false`. The firmware side of the voltage gate is done; only the hardware is left.

**Resume this if** the adapter fails the shut-off test below (the sensor turns ON while the car is off), or for any car that only has constant power.

## Why it's needed

* On some modern cars, every cabin fuse is constant power, and the in-car USB ports wake when a door opens.
* On constant power, the board ran 24/7 and eventually locked up, and opening a door in the garage could wake it and report an arrival.
* The voltage gate fixes both: the board only goes online while the alternator is charging, and deep sleeps the rest of the time.

## Option A: resistor divider (needs soldering)

Parts: 1.5MΩ ¼W 1% metal film resistor, 0.1µF ceramic capacitor, optional 3.3V zener (1N4728A), wire, a butt or Wago connector, heat shrink. DigiKey part numbers and links are in the spreadsheet:

* [Stackpole RNV14FAL1M50](https://www.digikey.com/en/products/detail/stackpole-electronics-inc/RNV14FAL1M50/2665259) (1.5MΩ)
* [TDK FG28X7R1H104KNT06](https://www.digikey.com/en/products/detail/tdk-corporation/FG28X7R1H104KNT06/5803202) (0.1µF)
* [onsemi 1N4728A](https://www.digikey.com/en/products/detail/onsemi/1N4728A/977183) (zener, optional)

The D1 Mini's header pins also need soldering on (they shipped loose in the bag).

```
12V add-a-fuse lead ──┬── 12V→USB adapter ── USB ── D1 Mini
                      │
                   [1.5MΩ]   at this end, in heat shrink
                      │
                 thin wire ── A0 ─┬─ [0.1µF] ─ GND (D1 Mini)
                                  └─ [3.3V zener, optional] ─ GND
D0 ──── RST
```

`config.h`: `ADC_FULL_SCALE_VOLTS 18.2f`. Any resistor from 1.2MΩ to 2MΩ works; full scale is roughly (R + 320k) / 100k, e.g. 1.5MΩ → 18.2V. **Don't use 1MΩ:** that caps the reading at about 13.2V, below the running voltage.

## Option B: no soldering

Parts: a NodeMCU ESP8266 with headers pre-soldered, a DC 0–25V voltage sensor module, female-to-female jumpers and a Wago 221 connector (see the "No-solder option" sheet).

```
12V add-a-fuse lead ──(Wago 221)──┬── 12V→USB adapter (as now)
                                  └── wire ──► module VCC terminal
car ground ─────────────────────────────────► module GND terminal

module S ──► A0      module – ──► GND      D0 ──► RST
```

`config.h`: `ADC_FULL_SCALE_VOLTS 16.0f`. Arduino IDE board: **NodeMCU 1.0 (ESP-12E Module)**. Tape the jumpers at each pin so vibration can't work them loose, and wrap the boards so exposed pins can't short.

## Firmware steps

1. In `config.h`, set `USE_VOLTAGE_GATE true` and `ADC_FULL_SCALE_VOLTS` for your option in the car's profile.
2. **Wire A0 before installing.** With nothing on A0 it reads 0V and the board sleeps forever.
3. Upload over USB. If the upload fails, pull the D0–RST jumper while flashing.

## Calibrate and test

1. With the engine running, measure the fuse-tap lead to chassis ground with a multimeter (expect 13.5–14.7V; 12.4–12.7V with the engine off).
2. Compare it to `sensor.carpresenceright_voltage` in Home Assistant and set `VOLTS_CALIBRATION` = multimeter ÷ reported.
3. Watch the voltage sensor over a few drives. The gate turns on above 13.1V and goes to sleep after 3 minutes below 12.9V (`engineOnVolts`, `engineOffVolts`, `engineOffHoldMs` in the sketch). Some cars' smart charging can dip the voltage while driving, so widen the hold time if the board drops out mid-drive.
4. Shut-off test: turn the car off, lock it, open a door a few minutes later. The sensor must stay OFF.
5. Departure test: start the car in the garage; the sensor should stay OFF. Drive out of range for 2+ minutes and come back; it should turn ON.

## Battery drain

In deep sleep the ESP8266 draws almost nothing. The 12V→USB adapter's idle current (often 5–20 mA) dominates, which is fine for a car that's driven regularly. If the car will sit for weeks, a lower-idle adapter is the thing to change.
