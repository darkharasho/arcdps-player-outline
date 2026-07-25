# Game-mode toggle (PvE / WvW) — design

## Goal

Let the user enable the self-marker per game mode: on just for WvW, just for
PvE, or both. Structured PvP is always off (never rendered), regardless of
settings.

## Mode detection (`src/core/mumble_data`)

GW2 packs a `MumbleContext` struct into the MumbleLink `context[256]` block.
Its `mapType` field is a `uint32` at byte offset 32 (after the 28-byte
`serverAddress` and the 4-byte `mapId`). Two new pure, unit-testable functions:

- `uint32_t read_map_type(const LinkedMem& m)` — reads the raw `mapType` from
  `m.context` at offset 32.
- `GameMode classify_map_type(uint32_t map_type)` returning
  `enum class GameMode { PvE, WvW, PvP }`:
  - **WvW** = map types 9, 10, 11, 12, 13, 14, 15, 19
    (EBG, borderlands, Obsidian Sanctum, Edge of the Mists, WvW lounge).
  - **PvP** = 2, 6, 8 (Heart of the Mists / tournament / user tournament).
  - **PvE** = everything else, including 0/unknown/loading (fail toward showing).

## Reader (`src/plugin/mumble_link`)

`MumbleReader::sample(core::AvatarState&, core::CameraState&, core::GameMode&)`
gains a `GameMode&` out-param, filled from the same live block via
`classify_map_type(read_map_type(lm))`.

## Config (`src/plugin/config`)

Two new bools, defaulting on, persisted to the ini and shown in options:

```cpp
bool show_in_pve = true;
bool show_in_wvw = true;
```

- Persist as `show_in_pve` / `show_in_wvw` (int 0/1), loaded in `load_config`.
- UI: below "Show self marker", a "Game modes" group with two checkboxes:
  `[x] PvE  [x] WvW`. No PvP checkbox — it is structurally off.

## Gating (`src/plugin/dllmain`)

After a successful `sample(...)`, before drawing, apply one rule:

- `PvP` → return (never draw).
- `WvW` → draw only if `show_in_wvw`.
- `PvE` (and unknown) → draw only if `show_in_pve`.

`g_cfg.enabled` remains the global master switch, checked first as today.
When gated off, call `reset_smoothing()` so the marker doesn't snap on re-entry.

## Testing

Add cases to `tests/core/test_mumble_parse.cpp`:

- `classify_map_type` returns PvE for open-world (5) and instance (4).
- Returns WvW for 9 and 15.
- Returns PvP for 2 and 6.
- Returns PvE for 0 / unknown high value (fallback).
- `read_map_type` pulls the right value from a populated `context`.
