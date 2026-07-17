# Prediction upstream

The client-side movement, weapon prediction, and prediction-state transfer code
is derived from [Velaron/cs16-client](https://github.com/Velaron/cs16-client).

The upstream implementation was reviewed at commit
`d6916e664ed037eaa2f00a0fab63555952961dde` (2026-07-15). The Steam GoldSrc
port keeps the upstream prediction flow while retaining its local ABI,
engine-callback, startup-tracing, and VGUI compatibility adaptations.

## Local mapping

- `pm_shared/` runs the shared movement simulation.
- `cl_dll/cs_wpn/cs_weapons.cpp` runs shared weapon code from
  `HUD_PostRunCmd` and writes the predicted result to `local_state_t`.
- `cl_dll/entity.cpp` transfers persistent prediction data through
  `HUD_TxferPredictionData` when the server acknowledges commands.
- `cl_dll/com_weapons.cpp` gates one-shot sounds and events with `g_runfuncs`.
- `dlls/wpn_shared/` supplies the shared Counter-Strike weapon behavior used
  by the client prediction build.

## Synced fixes

- `adf131d`: reject `MAX_AMMO_TYPES` as an invalid ammo-array index.
- `a5b152c`: use bounded formatting for prediction diagnostics.

Do not replace these files wholesale from upstream. The local versions contain
Steam GoldSrc ABI and engine compatibility work that is not present in the
Xash-oriented upstream tree.
