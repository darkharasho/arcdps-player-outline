## Project Context

An arcdps/C++ plugin for Guild Wars 2 that renders a persistent 2D outline over your own character — including when occluded by other players — so you stay findable in crowds and zergs. Inspired visually by the Minecraft "Player-Outline" mod (neilrush/Player-Outline). If reliable outlining proves too fragile, it falls back to a MumbleLink-anchored screen-space marker. It won't outline other players or NPCs, and the outline is a flat 2D silhouette, not true 3D-geometry edges.

## Goals

- Render a persistent 2D outline/silhouette over the player's own character in GW2
- Keep the outline visible even when the character is occluded by other players/geometry (see-through-crowd effect, like the Minecraft Player-Outline mod)
- Ship a MumbleLink-anchored screen-space marker as a fallback if outlining is too fragile
- Match the visual behavior of neilrush/Player-Outline as the north-star reference

## Out of scope

- Outlining other players or NPCs
- True 3D-geometry edge rendering (a flat 2D outline on the 3D model is sufficient)

## Suggested stack

- **C++ (arcdps plugin)** — arcdps plugins are native DLLs; provides ImGui overlay and DX11 device access
- **DirectX 11** — GW2's renderer; any stencil/depth/outline pass must hook into DX11
- **MumbleLink** — Provides player world position/orientation for the screen-space marker fallback without risky render hooks
