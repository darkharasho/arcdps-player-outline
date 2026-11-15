# Update notification — design

## Intent

Outline fixes are shipping several times a day, and users only find out by
visiting the releases page. The plugin should tell them when a newer release
exists. It only notifies; the user still downloads and replaces the DLL.

## Goals

- On load, find out whether a newer GitHub release exists.
- Show it in the options panel: `Update available: v0.4.4 (you have 0.4.3)`
  with the releases URL, plus a "Copy link" button.
- A "Check for updates on startup" checkbox, on by default, saved in the ini.

## Non-goals

- Downloading or installing anything (no self-update).
- Pop-ups, toasts or anything on screen outside the options panel.
- Checking again during a session.

## Anti-goals

- Never stall or crash the game. The check runs on its own short-lived thread
  with a 10 s timeout, and every failure is silent apart from a dim "update
  check failed" line in the options panel.
- No personal data sent. The only request is an anonymous GET with the user
  agent `arcdps_player_outline/<version>`.

## Approaches

1. **Ask the website where `/releases/latest` redirects (chosen).** One
   request to `https://github.com/darkharasho/arcdps-player-outline/releases/latest`
   with redirects off; the `Location` header ends in `/releases/tag/vX.Y.Z`.
   No JSON parsing, and it avoids api.github.com's limit of 60 unauthenticated
   calls an hour per IP, which every plugin on the connection shares. This is
   what arcdps-axipulse and arcdps-axigear already do.
2. **GitHub REST API (`/repos/.../releases/latest`).** Gives release notes too,
   but needs a JSON parser and hits the shared rate limit. Rejected.
3. **Port the axipulse self-updater.** Out of scope: the user chose notify
   only.

## Design

- `src/core/version.hpp` (pure, unit-tested):
  - `parse_release_tag(location)` extracts the tag after `/releases/tag/`.
  - `is_newer(remote, local)` compares `major.minor.patch`, ignoring a leading
    `v` and any suffix after the patch number. The build version comes from
    `git describe`, so a dev build reads like `0.4.3-2-gabc123`; that counts as
    0.4.3 and is not told about v0.4.3. An unparsable version on either side
    means "no update".
- `src/plugin/update_check.{hpp,cpp}` (Windows only):
  - `start(current_version)` spawns the thread. It uses WinHTTP
    (`WINHTTP_OPTION_REDIRECT_POLICY_NEVER`) and links `winhttp`; no new
    third-party code.
  - `status()` returns a small snapshot struct (state: idle, checking,
    up to date, available, failed; plus the tag), guarded by a mutex.
  - The thread is detached and touches only that guarded state, so plugin
    unload does not wait on the network.
- `Config` gains `check_updates = true`, written to and read from the ini.
- `dllmain.cpp`: `mod_init` calls `start()` when `check_updates` is set;
  `options_cb` draws the checkbox and the status line at the top of the panel.

## Left to the builder

- Exact wording and colours of the status line.
- WinHTTP call details and error mapping.

## Testing

- Native unit tests for `parse_release_tag` and `is_newer` (normal tags,
  trailing slash, missing tag, `v` prefix, dev suffix, equal, older, garbage).
- The network path is checked by hand in-game: a build with a lower version
  number must show the notice.
