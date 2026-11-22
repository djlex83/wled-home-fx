# wled-home-fx

[![Build with WLED](https://github.com/djlex83/wled-home-fx/actions/workflows/build.yml/badge.svg)](https://github.com/djlex83/wled-home-fx/actions/workflows/build.yml)

Five effects for [WLED](https://github.com/wled/WLED) that are made for the outside of a house: garages, carports,
driveways and porches. Each one does a single job that is otherwise hard to build from presets and playlists.

| Effect | What you see | Typical use |
| :-- | :-- | :-- |
| **Advent Wreath** | Candles with a real flicker, fir green and gold sparkles between them | Advent and Christmas, counts the candles from the date |
| **Runway** | Approach lights: a strobe runs to the end, steady edge lights, green threshold | Welcome light when a car arrives |
| **Battery** | A level bar that turns from red to yellow to green, with a charging flow | Home battery, car charge, water tank, any percentage |
| **Garage Door** | The strip fills (or empties) in the travel time of a door, with a blinking edge | Feedback while a door or gate is moving |
| **Notify** | A colour pulses a few times, then the light settles on a steady colour | Bin day, weather warnings, "the washing is done" |

All effects are 1D, work on any segment length, run on ESP32 and ESP8266 and do not need a palette unless noted.

## Install

You need WLED 16.0 or newer and a PlatformIO build of WLED
([how to compile WLED](https://kno.wled.ge/advanced/compiling-wled/)). WLED 16 loads usermods straight from GitHub.
Add this repository to `custom_usermods` in your `platformio_override.ini`:

```ini
[env:esp32dev]
custom_usermods =
  audioreactive
  https://github.com/djlex83/wled-home-fx.git
```

The official ESP32 release already contains `audioreactive`. Keep it in the list if you want the same features as the
release build. Then build and flash as usual, for example with `pio run -e esp32dev` and the update page of your WLED
(`http://<your-wled>/update`). [`platformio_override.sample.ini`](platformio_override.sample.ini) has a ready-made example.

### Effect numbers

Effects from usermods get their number when WLED starts. In the WLED app you simply pick them by name. For the JSON API
and for presets you need the number: open `http://<your-wled>/json/effects` and count from 0 to the name. Check the
numbers again after you add or remove other usermods.

## Effects

Slider and checkbox names below are the ones shown in the WLED app. In the JSON API they are `sx`, `ix`, `c1`, `c2`,
`c3`, `o1`, `o2` and `o3`.

### Advent Wreath

Candles spread evenly over the segment. Lit candles flicker like a flame and get redder when they dip. Unlit candles
can show a faint wax glow, and the gaps can show a wreath with slow sparkles.

| Control | Meaning | Default |
| :-- | :-- | :-- |
| Flicker speed (`sx`) | How lively the flames move | 96 |
| Flicker depth (`ix`) | How far a flame dips | 170 |
| Lit candles (`c1`) | 0 = none … 255 = all, in equal steps per candle | 64 |
| Candle size (`c2`) | Width of a candle within its slot | 110 |
| Candles (`c3`) | Number of candles, 0 = 4 | 4 |
| By date (`o1`) | Count the candles from the calendar (needs the WLED clock) | off |
| Wreath (`o2`) | Fill the gaps with colour 2 and sparkles in colour 3 | on |
| Wax (`o3`) | Faint glow on unlit candles | on |

Colours: **Flame**, **Wreath** (black = dark fir green), **Sparkle** (black = warm gold).

With **By date**, one candle burns from the first Advent Sunday, two from the second, and so on. All four burn from the
fourth Advent Sunday until 6 January. Outside that time no candle burns. Set the time zone and NTP in
*Config → Time & Macros*. If the clock is not set yet, the slider is used instead.

### Runway

Like the approach lights of an airport. A short strobe runs light by light to the end of the segment, then pauses.
Use the segment's **reverse** option to change the direction.

| Control | Meaning | Default |
| :-- | :-- | :-- |
| Strobe speed (`sx`) | How fast the strobe runs | 170 |
| Light spacing (`ix`) | LEDs from one light to the next (2 … 31) | 80 |
| Edge brightness (`c1`) | Brightness of the steady edge lights | 70 |
| Flash length (`c2`) | How many lights the afterglow of the strobe covers | 96 |
| Threshold (`o1`) | Last light steady in colour 3 | on |
| Edge lights (`o2`) | Steady edge lights in colour 2 between the strobes | on |

Colours: **Strobe**, **Edge** (black = amber), **Threshold** (black = green).

### Battery

A level gauge. Set the level with the **Level %** slider or from home automation. It works like WLED's *Percent*:
`ix` is the level from 0 to 100, higher values count as 100. The bar moves smoothly to a new level.

| Control | Meaning | Default |
| :-- | :-- | :-- |
| Speed (`sx`) | Speed of the bar and of the flow | 64 |
| Level % (`ix`) | Fill level in percent | 72 |
| Low warning (`c1`) | The bar pulses at or below this level (0 … 99 %) | 26 (= 10 %) |
| Flow length (`c2`) | Length of the charging / discharging flow | 96 |
| Charging (`o1`) | A bright flow runs towards the tip | off |
| Discharging (`o2`) | The flow runs back towards the start | off |
| Use palette (`o3`) | Take the colour from the palette at the level position | off |

Colours: **Empty** (the unfilled part, black by default). Without a palette the bar fades from red at 0 % through
yellow at 50 % to green at 100 %.

Example: show 72 % and charging on segment 0:

```json
{"seg":[{"id":0,"ix":72,"o1":true,"o2":false}]}
```

### Garage Door

The strip fills from the start to the end in the travel time of your door, while the front edge blinks. With
**Closing** it starts full and empties. When the time is over the strip stays full (opening) or empty (closing).

| Control | Meaning | Default |
| :-- | :-- | :-- |
| Blink speed (`sx`) | Blink rate of the leading edge | 180 |
| Travel time (`ix`) | 3 … 60 s (0 = 3 s, 255 = 60 s, 76 ≈ 20 s) | 95 (≈ 24 s) |
| Edge width (`c1`) | Width of the blinking edge (1 … 32 LEDs) | 24 |
| Closing (`o1`) | Empty the strip instead of filling it | off |
| From both ends (`o2`) | Fill from both ends towards the middle | off |
| Warning stripes (`o3`) | Moving stripes on the filled part | off |

Colours: **Edge**, **Door** (the filled part, black = dimmed edge colour), **Ahead** (not reached yet, usually black).

### Notify

A message in light. Colour 1 pulses a few times, then the light fades to colour 2 and stays there. Colour 2 black
means the light goes dark afterwards.

| Control | Meaning | Default |
| :-- | :-- | :-- |
| Pulse speed (`sx`) | Duration of one pulse (0.3 … 2.3 s) | 128 |
| Pulse depth (`ix`) | How dark the pulse gets between two peaks | 230 |
| Repeat every (`c1`) | Interval for **Repeat**, 10 … 120 s | 50 (≈ 32 s) |
| Fade to steady (`c2`) | Fade time after the last pulse, 0 … 2.5 s | 80 |
| Pulses (`c3`) | Number of pulses, 0 = 1 | 3 |
| Alternate colours (`o1`) | Every second pulse uses colour 3, for two messages at once | off |
| Repeat (`o2`) | Play the message again after the interval | off |

Colours: **Message**, **Steady**, **Message 2**.

### Playing once: Garage Door and Notify

WLED does not restart an effect that is already selected. These two effects therefore restart by themselves when

- the effect is selected,
- the segment or the whole light is switched on again,
- one of their settings or colours changes, or
- the effect did not run for more than a second.

So a preset with Garage Door or Notify plays from the start every time you apply it after the light was off or showed
something else. To play the same preset twice in a row, switch the light off for a moment in between.

## Development

The CI builds WLED v16.0.1 with this usermod for `esp32dev` and `nodemcuv2` on every push. To work on the effects,
point `custom_usermods` at your local copy (absolute path):

```ini
[env:esp32dev]
custom_usermods =
  symlink:///path/to/wled-home-fx
```

The effect code lives in [`src/home_fx.cpp`](src/home_fx.cpp). Effects follow the
[user_fx guide](https://github.com/wled/WLED/tree/main/usermods/user_fx) of WLED.

## License

[EUPL-1.2](LICENSE), the same licence as WLED.
