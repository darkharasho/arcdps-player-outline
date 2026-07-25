# Hide marker when map open / unfocused — design

## Goal

Hide the self-marker when the full-screen world map is open (and optionally when
the game is alt-tabbed / unfocused), so it doesn't clutter the map or draw over
other windows. Also respect arcdps's own UI-hide key.

## uiState (`src/core/mumble_data`)

GW2's MumbleContext carries a `uiState` bitfield: a `uint32` at byte offset 48
of `context` (after serverAddress[28] + mapId + mapType + shardId + instance +
buildId, each uint32). Add:

- `uint32_t read_ui_state(const LinkedMem& m);` — reads offset 48.
- `bool ui_map_open(uint32_t s);`     — bit 0 (IsMapOpen).
- `bool ui_game_focused(uint32_t s);` — bit 3 (GameHasFocus).

## Reader (`SessionInfo`)

Extend the struct:

```cpp
struct SessionInfo {
    core::GameMode mode = core::GameMode::PvE;
    core::GameRace race = core::GameRace::Unknown;
    bool map_open = false;
    bool focused  = true;
};
```

`sample()` fills `map_open`/`focused` from `read_ui_state(lm)`.

## Config (`src/plugin/config`)

```cpp
bool hide_when_map_open  = true;
bool hide_when_unfocused = true;
```

Persist as `hide_when_map_open` / `hide_when_unfocused`; two checkboxes in the
"Behavior" section.

## Gate (`src/plugin/dllmain`)

`imgui_cb` currently ignores its second `hide` argument. Wire it up, then add the
map/focus early-outs. Each returns after `reset_smoothing()` so the marker
doesn't snap on re-entry:

- arcdps `hide` flag set → hide (respects arcdps's UI-hide key).
- `hide_when_map_open && session.map_open` → hide.
- `hide_when_unfocused && !session.focused` → hide.

These sit alongside the existing `enabled` + per-mode gate.

## Testing (`tests/core/test_mumble_parse.cpp`)

- `read_ui_state` pulls the value written at context offset 48.
- `ui_map_open(0x1)` true, `ui_map_open(0x8)` false.
- `ui_game_focused(0x8)` true, `ui_game_focused(0x1)` false.
- Combined bits (e.g. `0x9`) decode both flags independently.
