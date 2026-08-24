# Star Watch

Hold the watch up to the night sky like a window. Colored circles mark the Sun, Moon, planets, and stars down to magnitude 4.5 (on black-and-white Pebble 2 Duo they render as white marks). A crosshair in the center, a leader line, and a name along the bottom identify the nearest named object.

Location comes from the phone’s GPS. The watch uses its compass and accelerometer for pointing. Planet positions are computed on the phone; the star catalog lives on the watch.

Opens in **Manual Targeting**: the name bar follows whatever is nearest the crosshair. Object Targeting is a separate mode from the Select menu.

## Controls

- **Up** — backlight. On Pebble Time 2: white → red (night) → off. On Pebble 2 Duo and Round 2: on → off (those watches have a white LED, not RGB).
- **Select** — menu (arrow icon): Object Targeting, Settings, Compass %, GPS %
- **Down** — Manual Targeting → zodiac → all stick figures (and back). From Object Targeting, Down returns to Manual Targeting.
- Hold the watch in front of you, screen toward your eyes, back toward the sky
- If the compass needs calibration, tilt the watch to roll the ball around the ring (Back dismisses that screen)

Long names ping-pong in the name bar and in menus when they do not fit.

## Watches

| Watch | SDK | Notes |
| --- | --- | --- |
| Pebble Time 2 | `emery` | Primary. Color, 200×228, RGB backlight. Tested on hardware. |
| Pebble 2 Duo | `flint` | Color-off build: 144×168 B/W, compact UI, on/off backlight only. Not hardware-tested. |
| Pebble Round 2 | `gabbro` | Color, 260×260 round, icons inset for the circular screen, on/off backlight only. Not hardware-tested. |

Needs a paired phone for the first GPS fix and for planet / ISS updates. The Pebble 2 Duo build is tight on RAM (star direction cache omitted so the app can allocate menus); expect slightly heavier sky redraws on that watch.

Other Pebbles (Classic, Time, Time Round, original Pebble 2) are not targeted: too little RAM, no compass, and/or a round 180×180 layout this app does not support.

## Build (Windows)

The Pebble SDK does not run natively on Windows. Use WSL Ubuntu:

```bash
# once
sudo apt update && sudo apt install -y build-essential python3
curl -LsSf https://astral.sh/uv/install.sh | sh
uv tool install pebble-tool --python 3.13
pebble sdk install latest

cd "/mnt/e/Mark/webdev/Pebble watch/Star Watch"
npm install
pebble build
pebble install --emulator emery
```

On a physical watch, enable Dev Connect in the Pebble phone app, then:

```bash
pebble install --cloudpebble
```

Regenerate the packed star catalog (optional):

```bash
python3 tools/gen_catalog.py
```
