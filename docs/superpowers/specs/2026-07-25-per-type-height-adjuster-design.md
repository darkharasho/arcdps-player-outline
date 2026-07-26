# Per-type height adjuster + hover-above-head defaults

Date: 2026-07-25

## Problem

The screen-space self-marker anchors its head-based elements (pip, chevron) off the
player's race height (`race_head_m` + a single global `height_nudge`). Three issues:

1. The `height_nudge` adjuster is global — it shifts every head-anchored style at once.
   Pip and chevron can't be tuned independently.
2. After fitting to race, the marker rests too low: the pip sits at mid-body
   (`0.5 × head_h`) and the chevron sits right at the crown, so neither reads as a
   clear "here I am" hover above the head.
3. The per-race head defaults are too short for Charr, Norn, Human, and Sylvari
   (Asura is fine).
4. There is no way to see the actual resulting height without toggling *Fit height to
   race* off to reveal the manual sliders.

## Goals

- Split the single adjuster into **two** per-type nudges: a **pip** nudge and a
  **chevron** nudge. The chevron nudge is shared by both chevron styles (standalone
  Chevron and Ring+chevron); the pip nudge is its own.
- Make both elements hover **clearly above the head** by default, for every race.
- Raise the per-race head defaults for Charr, Norn, Human, Sylvari.
- Always show a live readout of the effective height, and expose manual per-type base
  heights when auto-fit is off.

## Non-goals

- No change to the silhouette-glow aura sizing (`char_height` stays as-is; it is body
  height, not a head anchor).
- No change to ground ring, beam, fade, off-screen arrow, or gating behavior.

## Placement model

Both pip and chevron anchor on the **head crown** and then add a per-type offset:

```
base_h                = fit ? fitted_race_head(race) : per_type_manual_base
effective_pip_h       = base_h + pip_nudge
effective_chevron_h   = base_h + chevron_nudge
```

- `fitted_race_head(race)` = `race_head_m[idx]` (Unknown → Human), as today, but **no
  longer** includes a global nudge.
- Replaces: the pip's `0.5 × head_h`, the Ring+chevron's hardcoded `2.4`, and the
  standalone chevron's `head_offset` fallback.
- The chevron nudge drives **both** `MarkerStyle::Chevron` and the overhead V of
  `MarkerStyle::RingChevron`. The pip nudge drives `MarkerStyle::RingPip`.

## Config changes (`config.hpp` / `config.cpp`)

Remove:
- `float height_nudge`
- `float head_offset` (repurposed below)

Add / change:
- `float pip_nudge     = 0.20f;`   // meters above base for the pip (clear hover)
- `float chevron_nudge = 0.45f;`   // meters above base for the chevron (floats higher)
- `float pip_manual_m     = 2.20f;` // base used only when fit-to-race is OFF
- `float chevron_manual_m = 2.20f;` // base used only when fit-to-race is OFF
  (this is the old `head_offset` role, renamed)

Raise `race_head_m` defaults (feet→crown, meters):
`{Asura 1.05, Charr 2.35, Human 2.05, Norn 2.75, Sylvari 2.00}`

Ranges / sanitize:
- `pip_nudge`, `chevron_nudge`: clamp `-0.5 … 1.5`.
- `pip_manual_m`, `chevron_manual_m`: clamp `0.0 … 3.5`.
- `race_head_m[i]`: clamp `0.5 … 3.5` (unchanged).

Persistence (`save_config` / `load_config`):
- Write/read new keys: `pip_nudge`, `chevron_nudge`, `pip_manual_m`, `chevron_manual_m`.
- Drop `height_nudge` and `head_offset` keys. Unknown keys in an old ini are ignored by
  the existing parser, so stale files load cleanly and re-save in the new format.

## Options UI (`draw_options`)

Under **Fit height to race**:

- **Auto ON:** show the per-race head table (as today, with the "(you)" highlight) plus
  `pip_nudge` and `chevron_nudge` sliders (`%+.2f m`).
- **Auto OFF:** show `pip_manual_m` and `chevron_manual_m` base sliders (`%.2f m`) plus
  the same two nudge sliders.
- **Always (both modes):** a live readout line for the detected race, e.g. for a Human
  (2.05 + 0.20 / 2.05 + 0.45): `Pip -> 2.25 m   Chevron -> 2.50 m`, computed from the
  same `effective_*` formula the
  renderer uses. This is the "actual settings" view that no longer requires toggling.

The per-style sliders in the top switch block stay: standalone Chevron and Ring+chevron
still expose `chevron_size` (pixel size of the V); Ring+pip has no size slider (fixed
radius). The old manual `Height above (m)` slider inside the Chevron case is removed —
height is now handled uniformly by the fit/manual block below.

## Renderer changes (`dllmain.cpp`)

- Compute `base_h` once: `fit ? fitted_race_head(g_cfg, session.race) : <per-type manual>`.
  Because the manual base differs per type when fit is off, compute
  `pip_base` and `chevron_base` separately.
- `MarkerStyle::Chevron` (standalone): anchor world Y = `chevron_base + chevron_nudge`.
- `MarkerStyle::RingPip`: pip world Y = `pip_base + pip_nudge`.
- `MarkerStyle::RingChevron`: chevron world Y = `chevron_base + chevron_nudge`.
- Add a small helper (in `config.cpp`, mirroring `fitted_head_height`) so the UI readout
  and the renderer share one source of truth:
  - `float effective_pip_height(const Config&, GameRace)`
  - `float effective_chevron_height(const Config&, GameRace)`
  Each resolves base (fit vs manual) + the matching nudge. `fitted_head_height` is
  removed (superseded) or kept only if still referenced; replace call sites.

## Testing

- No new pure-core math, so no `src/core` tests required.
- If a lightweight native test exists for config load/save round-trip, extend it to
  cover the new keys and the dropped keys. Otherwise, verification is the in-game
  checkpoint: darkharasho tests every build on Bazzite/Proton.
- Manual in-game checks: pip and chevron each hover clearly above the head across races;
  the two nudges move only their own element; the readout matches what's on screen; the
  manual sliders appear only when auto-fit is off.

## Open tuning knobs

The proposed race numbers and the two default hover values (0.20 / 0.45 m) are starting
points; darkharasho tunes them in-game and they are all editable at runtime.
