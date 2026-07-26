# Checkbox-based marker elements

Date: 2026-07-25

## Problem

The self-marker's visual style is chosen from a single dropdown (`MarkerStyle`
combo) offering six fixed presets, two of which are already fixed combinations
(`RingPip` = ring + pip, `RingChevron` = ring + chevron). Users cannot build
their own combination — e.g. silhouette glow + overhead chevron + pip at once, or
ring + beam. One preset is drawn per frame via a `switch (style)`.

## Goal

Replace the preset dropdown with **five independent element toggles** that can be
combined freely. Each element draws independently every frame; any subset (including
none) is valid.

The five elements:
- **Ground ring** — projected floor circle/ellipse
- **Silhouette glow** — soft aura hugging the body
- **Overhead chevron** — the V above the head
- **Beam** — vertical light pillar
- **Head pip** — floating dot above the head

## Non-goals

- No change to per-element visual tuning (ring size, glow width/amount, chevron
  size, beam height/width) or to the per-type pip/chevron height adjusters — those
  carry over unchanged.
- No per-element color/opacity/outline — Color, Opacity, and Outline stay global,
  shared across all drawn elements, as today.
- No new element types beyond the five that already exist.

## Config model (`config.hpp` / `config_io.cpp`)

Remove the runtime `MarkerStyle style` field. Add five bools:

```cpp
    bool show_ring    = true;    // ground ring (default on — matches old GroundRing default)
    bool show_glow    = false;   // silhouette glow
    bool show_chevron = false;   // overhead chevron
    bool show_beam    = false;   // vertical beam
    bool show_pip     = false;   // head pip
```

All existing element-tuning fields stay: `ring_radius`, `glow_width`, `glow_amount`,
`char_height`, `chevron_size`, `beam_height`, `beam_width`, and the height-fit fields
(`fit_height_to_race`, `race_head_m[5]`, `pip_nudge`, `chevron_nudge`, `pip_manual_m`,
`chevron_manual_m`).

The `MarkerStyle` enum is retained ONLY as an internal mapping used during load
migration (see below); no runtime code branches on it.

### Persistence

`save_config` writes the five bools:

```
show_ring, show_glow, show_chevron, show_beam, show_pip   (each as %d 0/1)
```

and no longer writes `style`.

`load_config` parses the five `show_*` keys. For back-compat with pre-existing inis
that only have `style=N`:
- Parse `style=N` into a transient int when seen.
- Track whether any `show_*` key was seen during the parse.
- After the parse loop, if NO `show_*` key was present but a `style` value was, apply
  the migration mapping to set the bools:
  | old style        | N | ring | glow | chevron | beam | pip |
  |------------------|---|------|------|---------|------|-----|
  | GroundRing       | 0 | ✓    |      |         |      |     |
  | SilhouetteGlow   | 1 |      | ✓    |         |      |     |
  | Chevron          | 2 |      |      | ✓       |      |     |
  | Beam             | 3 |      |      |         | ✓    |     |
  | RingPip          | 4 | ✓    |      |         |      | ✓   |
  | RingChevron      | 5 | ✓    |      | ✓       |      |     |
- If neither `show_*` nor `style` was present (fresh/empty ini), the struct defaults
  (`show_ring = true`) stand.

`sanitize` needs no change for the bools (bools can't be out of range). Drop the
`(int)c.style` range check.

## Renderer (`dllmain.cpp`)

Replace the `switch (g_cfg.style)` with independent, always-evaluated element draws
in a fixed layering order (back to front): **ground ring → silhouette glow → beam →
pip → chevron**.

Anchoring unification (the one real refactor):
- Smooth the **feet** anchor once per frame: `sx = g_fx.filter(feet.x, dt)`,
  `sy = g_fy.filter(feet.y, dt)`, and `ox = sx - feet.x`, `oy = sy - feet.y`.
  (Today the standalone-chevron path smooths the head point directly; that special
  case goes away.)
- Ground ring: unchanged — `build_ground_ring(... ox, oy)`.
- Silhouette glow: unchanged math, anchored at `sx, sy` as today (uses
  `fitted_head_height` when fit is on, else `char_height`). The glow's `g_fh` body-
  height smoothing stays.
- Beam: `world_to_screen(feet + {0, beam_height, 0})` → `top_y = behind ? sy-200 :
  top.y + oy`; base at `sx, sy`.
- Pip (when `show_pip`): `world_to_screen(feet + {0, effective_pip_height, 0})`;
  draw at `hp.x + ox, hp.y + oy` when not behind.
- Chevron (when `show_chevron`): `world_to_screen(feet + {0, effective_chevron_height,
  0})`; draw at `hp.x + ox, hp.y + oy`, with the existing behind-camera fallback
  (`sx, sy-60`). This is the current `RingChevron` head logic, now used for the sole
  chevron path.
- If NO element is enabled, the function still runs smoothing harmlessly and draws
  nothing. (The existing top-level gates — `enabled`, mode, map-open, unfocused,
  off-screen — are unchanged and still short-circuit before this block.)

The off-screen edge-arrow branch (when the player is behind/past the screen edge) is
unchanged and still governed by `offscreen_arrow`.

## Options UI (`config.cpp` `draw_options`)

Replace the Style combo with a "Marker elements" block:

```
Marker elements
[x] Ground ring
[ ] Silhouette glow
[ ] Overhead chevron
[ ] Beam
[ ] Head pip
```

Under each CHECKED element, show that element's own settings (moved out of the old
per-style `switch`), only while checked:
- Ground ring → `Ring size (m)` slider.
- Silhouette glow → `Width` and `Glow` sliders; `Height fit (m)` slider only when
  `!fit_height_to_race` (as today).
- Overhead chevron → `Chevron size` slider.
- Beam → `Beam height (m)` and `Beam width` sliders.
- Head pip → (no size slider today; pip radius is fixed — no per-element control here).

Color / Opacity / Outline stay global at the top, unchanged.

In the height-fit block: show the `Pip nudge` slider only when `show_pip`, and the
`Chevron nudge` slider only when `show_chevron`. The race table and manual base
sliders show as today. The live readout shows a line per enabled head element (pip
and/or chevron); if neither is enabled, the readout is omitted.

## Testing

- Extend `tests/core/test_config.cpp` (native doctest) with:
  - Round-trip of the five `show_*` bools through `save_config`/`load_config`.
  - Migration: write an ini containing only `style=4` (RingPip) and assert
    `show_ring == true && show_pip == true` and the others false after load.
  - Migration precedence: an ini with BOTH `style=1` and explicit `show_*` keys keeps
    the explicit `show_*` values (new keys win; no migration overwrite).
  - Empty ini leaves defaults (`show_ring == true`, others false).
- Renderer and UI changes are ImGui/Windows-coupled → verified by the mingw DLL
  build plus in-game checkpoint (darkharasho, Bazzite/Proton): confirm each checkbox
  toggles its element independently, combinations layer correctly (ring under, chevron
  on top), and old configs load with the equivalent elements enabled.

## Open notes

- Layering order (ring → glow → beam → pip → chevron) approved as the fixed z-order.
- Unchecking all elements draws nothing (no snap-back to a default); the top-level
  "Show self marker" checkbox remains the master gate.
