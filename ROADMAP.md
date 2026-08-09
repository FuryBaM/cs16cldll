# Roadmap

This roadmap tracks the work required to turn the current experimental Steam
GoldSrc client replacement into a stable, maintainable release. It is ordered by
risk and dependency rather than by estimated completion date.

## Project boundaries

The project targets the original 32-bit Steam GoldSrc engine and preserves its
native client ABI. It remains a replacement for
`cstrike/cl_dlls/client.dll`, not a standalone game, server DLL, Xash3D port, or
mobile client.

Changes should preserve compatibility with stock Counter-Strike 1.6 resources
and servers. Optional integrations must fail gracefully when their runtime
interfaces are unavailable.

## Current foundation

The following foundation is already in place:

- [x] Native PE32/x86 build with all 44 expected Steam GoldSrc client exports.
- [x] CI build and ABI/import verification.
- [x] VGUI1 integration and a VGUI2 viewport with a VGUI1 fallback.
- [x] Team, class, and buy menus.
- [x] Shared movement and weapon prediction through `HUD_PostRunCmd`.
- [x] Counter-Strike weapon events and studio model rendering.
- [x] HUD, scoreboard, radar, spectator UI, voice indicators, and widescreen scope.
- [x] UTF-8/CP1251-aware HUD and menu text rendering.
- [x] Steam avatars and Rich Presence.
- [x] Optional Discord Rich Presence.

## Phase 1 — Stabilization

Goal: make the existing feature set dependable before expanding it.

- [ ] Build a repeatable in-game smoke-test checklist covering startup, local
  play, multiplayer connection, map changes, disconnects, and shutdown.
- [ ] Test every stock weapon for firing, reload, recoil, animation, sound, and
  prediction reconciliation under latency and packet loss.
- [ ] Fix or replace the broken `CL_MuzzleFlash` path.
- [ ] Audit server-message readers for malformed or truncated payloads and make
  failure behavior deterministic.
- [ ] Finish or explicitly retire the incomplete `HudTextPro` and
  `HudTextArgs` paths.
- [ ] Replace the fallback handling for unknown HUD text effects with documented,
  bounded behavior.
- [ ] Remove stale health-state TODOs after confirming the local state consumers.
- [ ] Remove the temporary `numericalmenu`, `numericalmenu_clientonly`, and
  `checkscoreboard` compatibility hacks when their callers no longer need them.
- [ ] Verify repeated map changes and reconnects do not leak VGUI panels, Steam
  avatar textures, temporary entities, or Discord handles.

## Phase 2 — Stock UI completeness

Goal: make the replacement behave consistently across the complete stock client
flow.

- [ ] Inventory every stock `VGUIMenu` ID emitted by Counter-Strike 1.6.
- [ ] Implement the remaining stock VGUI menus or document why a text-menu
  fallback is required.
- [ ] Keep an unsupported menu local to that menu instead of disabling VGUI
  menus for the entire session where possible.
- [ ] Replace hard-coded English menu labels with stock localization tokens.
- [ ] Validate mouse, keyboard, number-key, Escape, Back, autobuy, and rebuy
  behavior for every menu.
- [ ] Test VGUI1 and VGUI2 layouts at 4:3, 16:9, 16:10, and ultrawide
  resolutions.
- [ ] Remove hard-coded HUD decoration colors and move them into a theme or
  resource definition.
- [ ] Define predictable handling for plain-text and HTML MOTDs without
  displaying raw markup.
- [ ] Verify scoreboard, radar, spectator cards, voice indicators, and Steam
  avatars with full 32-player servers.

## Phase 3 — Compatibility and diagnostics

Goal: make failures diagnosable and isolate engine-version differences.

- [ ] Add a documented compatibility matrix for supported Steam GoldSrc builds,
  Windows versions, and common server configurations.
- [ ] Test clean installs with missing optional VGUI2, Steam, and Discord
  interfaces to confirm graceful fallback.
- [ ] Add a concise runtime diagnostics command that reports ABI, viewport,
  prediction, Steam, Discord, and active fallback status.
- [ ] Make startup tracing opt-in for release builds and keep logs bounded.
- [ ] Validate localization with English, Russian, UTF-8, and CP1251 server text.
- [ ] Test demo recording/playback, spectator transitions, HLTV, and reconnects.
- [ ] Decide whether the CS:CZDS-only `EV_CreateExplo` event should be
  implemented, isolated as optional compatibility code, or removed from the
  CS 1.6 build.

## Phase 4 — Maintainability

Goal: reduce the cost and risk of future changes.

- [ ] Separate client-required sources from inherited HLSDK server/tool code in
  project organization and documentation.
- [ ] Document the ownership and adaptation status of upstream-derived files.
- [ ] Define a review procedure for syncing prediction and shared-weapon fixes
  without overwriting Steam-specific adaptations.
- [ ] Enable a warning baseline for project-owned code and prevent new warnings
  from entering CI.
- [ ] Add focused tests for message parsing, localization conversion, menu
  routing, and resource parsing.
- [ ] Add Debug and Release CI coverage while keeping Release Win32 ABI
  verification mandatory.
- [ ] Document contribution, coding-style, regression-report, and release
  procedures.

## Version 1.0 release criteria

Version 1.0 is ready when all of the following are true:

- [ ] Release Win32 builds reproducibly and passes the export/import verifier.
- [ ] The client completes the smoke-test matrix without crashes or stuck input.
- [ ] Stock weapons and grenades pass prediction and event regression testing.
- [ ] Stock team, class, buy, scoreboard, radar, voice, and spectator flows work.
- [ ] Unsupported optional interfaces fall back without breaking the session.
- [ ] Known compatibility limitations are documented.
- [ ] Installation, restoration, troubleshooting, and release notes are current.
- [ ] A release candidate has been tested on local play and multiple public or
  dedicated servers with `-insecure`.

## Suggested test matrix

| Area | Minimum coverage |
| --- | --- |
| Build | Debug x86, Release x86, clean checkout |
| Display | 4:3, 16:9, 16:10, ultrawide |
| Session | local map, listen server, dedicated server, reconnect, map change |
| Network | low latency, high latency, packet loss |
| UI | VGUI2 enabled, VGUI1 fallback, text-menu fallback |
| Gameplay | all weapons, grenades, buy flow, death, spectator, HLTV |
| Text | English, Russian, UTF-8, CP1251, long server messages |
| Optional APIs | Steam available/unavailable, Discord enabled/disabled |

## Backlog policy

Keep this file focused on milestones and release criteria. Concrete defects and
small implementation tasks should be tracked as GitHub issues and linked from
the relevant phase. Mark an item complete only after the corresponding behavior
has been tested in the original Steam GoldSrc engine.
