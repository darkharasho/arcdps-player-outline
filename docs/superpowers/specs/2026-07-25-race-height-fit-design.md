# Race detection + per-race height fit — design

## Goal

Head-anchored markers (chevron, ring+chevron, ring+pip, silhouette) use fixed
height constants tuned for a human, so on a tall race (Norn) the chevron lands
at chest height. Detect the player's race from MumbleLink and fit marker
heights to it, with an editable per-race table for fine control.

## Race detection (`src/core/mumble_data`)

GW2's identity JSON (already parsed for `fov`) carries a `"race"` integer:
0 Asura, 1 Charr, 2 Human, 3 Norn, 4 Sylvari. Add:

- `enum class GameRace { Asura, Charr, Human, Norn, Sylvari, Unknown };`
- `GameRace parse_identity_race(const char* utf8_identity);` — reads the `race`
  int; missing key or value outside 0–4 → `Unknown`.

## Reader (`src/plugin/mumble_link`)

Fold the game-mode out-param into a small struct and add race:

```cpp
struct SessionInfo { GameMode mode = GameMode::PvE; GameRace race = GameRace::Unknown; };
```

`MumbleReader::sample(AvatarState&, CameraState&, SessionInfo&)` fills both from
the live block (`classify_map_type(read_map_type(lm))` and
`parse_identity_race(identity)`).

## Config (`src/plugin/config`)

```cpp
bool  fit_height_to_race = true;
float race_head_m[5] = {1.00f, 2.20f, 1.85f, 2.55f, 1.80f}; // Asura,Charr,Human,Norn,Sylvari
float height_nudge   = 0.0f;   // meters, applied on top of the fitted height
```

- Persist as `fit_height_to_race`, `race_head_asura`/`_charr`/`_human`/`_norn`/
  `_sylvari`, and `height_nudge`.
- Sanitize: each `race_head_m` clamped to 0.5–3.5 m; `height_nudge` to -1.0–1.0.
- `Unknown` race resolves to the Human entry (index 2).

Helper (config.cpp): `float fitted_head_height(const Config&, GameRace)` returns
`race_head_m[idx(race)] + height_nudge`.

## Applying the height (`src/plugin/dllmain`)

Compute `H = fitted_head_height(cfg, session.race)` when `fit_height_to_race`.
Then:

- Chevron & Ring+chevron head anchor → `H` (replaces `head_offset` / the
  hardcoded `2.4`).
- Ring+pip → pip at `0.5f * H` (replaces hardcoded `1.2`).
- Silhouette glow → use `H` in place of `cfg.char_height`.

When `fit_height_to_race` is off, behavior is unchanged from today (the
`head_offset` / `char_height` sliders and the 2.4 / 1.2 constants).

The last sampled race is cached in a dllmain global so `draw_options` can
highlight the detected row.

## UI (`draw_options`)

Signature gains the detected race: `draw_options(Config&, core::GameRace detected)`.

- "Fit height to race" checkbox (default on).
- When on: show a "Height nudge (m)" slider (-1..+1) and a 5-row editable table
  (`ImGui::DragFloat` per race, 0.5–3.5 m), with the detected race row prefixed
  (e.g. `> Norn`) / colored. Hide the per-style `head_offset` / `char_height`
  sliders.
- When off: today's per-style sliders reappear; no table.

## Testing (`tests/core/test_mumble_parse.cpp`)

- `parse_identity_race` returns the right race for each id 0–4.
- Missing `"race"` key → Unknown; out-of-range (e.g. 9) → Unknown.
- A Norn identity string maps to a taller `race_head_m` entry than Human.
