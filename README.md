# Star Watch

Hold the watch up to the night sky like a window. Colored circles mark the Sun, Moon, planets, and stars to magnitude 2.5, plus named stars to magnitude 4.5 (on black-and-white Pebble 2 Duo they render as white marks). A crosshair in the center, a leader line, and a name along the bottom identify the nearest named object.

Location comes from the phone’s GPS. The watch uses its compass and accelerometer for pointing, or — on touchscreen watches — optional finger-drag panning. Planet positions are computed on the phone; the star catalog lives on the watch.

Opens in **Manual Targeting**: the name bar follows whatever is nearest the crosshair. Object Targeting is a separate mode from the Select menu.

## Controls

- **Up** — backlight. On Pebble Time 2: white → red (night) → off. On Pebble 2 Duo and Round 2: on → off (those watches have a white LED, not RGB).
- **Select** — menu (arrow icon): Object Targeting, Settings, Compass %, GPS %
- **Down** — Manual Targeting → zodiac → all stick figures (and back). From Object Targeting, Down returns to Manual Targeting.
- Hold the watch in front of you, screen toward your eyes, back toward the sky
- On Pebble Time 2 and Round 2, Settings → **Touch mode** (off by default) ignores the compass and accelerometer and lets you drag the sky with a finger. Pebble 2 Duo has no touchscreen, so that setting is omitted.
- Settings → **Ecliptic** (on by default) draws Earth’s orbital plane as a blue ring, the same great circle the Sun and planets follow out to Neptune. On Pebble 2 Duo it is a white ring.
- If the compass needs calibration, tilt the watch to roll the ball around the ring (Back dismisses that screen)

Long names ping-pong in the name bar and in menus when they do not fit.

## Watches

| Watch | SDK | Notes |
| --- | --- | --- |
| Pebble Time 2 | `emery` | Primary. Color, 200×228, RGB backlight, touchscreen. Tested on hardware. |
| Pebble 2 Duo | `flint` | Color-off build: 144×168 B/W, compact UI, on/off backlight only. Compass pointing only (no touchscreen). Not hardware-tested. |
| Pebble Round 2 | `gabbro` | Color, 260×260 round, icons inset for the circular screen, on/off backlight, touchscreen. Not hardware-tested. |

Needs a paired phone for the first GPS fix and for planet / ISS updates. The Pebble 2 Duo build is tight on RAM (star direction cache omitted so the app can allocate menus); expect slightly heavier sky redraws on that watch.

Other Pebbles (Classic, Time, Time Round, original Pebble 2) are not targeted: too little RAM, no compass, and/or a round 180×180 layout this app does not support. A touch-pan setting does not change that — only Time 2 and Round 2 have a touchscreen, and both are already supported. There is currently no shipping Pebble that has a touchscreen but no compass.
