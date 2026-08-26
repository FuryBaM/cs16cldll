# CS 1.6 Steam GoldSrc Client

An experimental, open-source replacement for Counter-Strike 1.6's
`cstrike/cl_dlls/client.dll`, built specifically for the original 32-bit Steam
GoldSrc engine.

The DLL keeps the native GoldSrc client ABI while restoring Counter-Strike
client features such as the original VGUI, weapon prediction, the HUD,
scoreboard, radar, spectator interface, voice indicators, and weapon events.
It is **not** a Xash3D or mobile client build and does not depend on Xash-specific
engine, renderer, or UI APIs.

> **This is the only client that compiles for the original Steam GoldSrc engine
> and works on multiplayer servers without crashes.**

[Download the latest release](https://github.com/FuryBaM/cs16-goldsrc-client/releases/latest)
· [Browse the source](https://github.com/FuryBaM/cs16-goldsrc-client)
· [View the roadmap](ROADMAP.md)
· [Report an issue](https://github.com/FuryBaM/cs16-goldsrc-client/issues)

## Steam Guides

If you prefer a step-by-step setup guide with screenshots, or want to rate and
share the project within the Steam Community, check out the official guides:

- 🇬🇧 [English Setup Guide on Steam](https://steamcommunity.com/sharedfiles/filedetails/?id=3767117515)
- 🇷🇺 [Russian Setup Guide on Steam](https://steamcommunity.com/sharedfiles/filedetails/?id=3767105222)

## Important safety notice

This project replaces a game binary. Back up the original `client.dll` before
installing anything.

Use `-insecure` while developing or testing this client. Do not join
VAC-secured servers with a modified client module. Test it locally first.

## Highlights

- Native PE32/x86 Steam GoldSrc ABI with all 44 expected client exports.
- No dependency on `xash.dll`, `mainui.dll`, or mobile/render APIs.
- Original Steam `vgui.dll` and `vgui2.dll` integration with a VGUI1 fallback.
- VGUI2 team, model, and buy menus using the original CS 1.6 resources.
- Client-side weapon prediction and reconciliation through `HUD_PostRunCmd`.
- Restored Counter-Strike studio renderer, gait animation, and weapon events.
- Original CS HUD sprites and fonts.
- Steam avatars in the scoreboard and spectator HUD.
- Improved kill feed, voice speaker display, overview radar, and spectator UI.
- Correct widescreen sniper scope rendering.
- Steam Rich Presence and optional Discord RPC without an extra Discord DLL.
- Static MSVC runtime in Release builds.

## Install a release

### 1. Download the client

Download the newest Win32 release from the
[Releases page](https://github.com/FuryBaM/cs16-goldsrc-client/releases/latest)
and extract it to a temporary folder.

### 2. Locate Counter-Strike 1.6

In Steam:

1. Open **Library**.
2. Right-click **Counter-Strike** and select **Properties**.
3. Open **Installed Files**.
4. Select **Browse**.

The game directory is normally named `Half-Life`. The file being replaced is:

```text
Half-Life\cstrike\cl_dlls\client.dll
```

### 3. Back up the original client

Close Counter-Strike completely, then copy the existing file to a safe name:

```text
Half-Life\cstrike\cl_dlls\client.dll
    ->
Half-Life\cstrike\cl_dlls\client.dll.original
```

Do not skip this step. The backup provides the fastest way to return to Valve's
original client.

### 4. Install the replacement

Copy the downloaded `client.dll` to:

```text
Half-Life\cstrike\cl_dlls\client.dll
```

Confirm replacement when Windows asks.

Do not copy `vgui.dll`, `vgui2.dll`, or `SDL2.dll` from Xash3D. This client uses
the versions shipped with the Steam installation.

### 5. Test locally

Add these launch options in Steam:

```text
-insecure -dev -console
```

Start Counter-Strike and run this command in the console:

```text
map de_dust2
```

Check movement, mouse input, team selection, the buy menu, firing, weapon
switching, HUD, radar, scoreboard, spectator mode, and voice chat before joining
any multiplayer server.

## Restore the original Steam client

Close Counter-Strike before restoring the DLL.

### Restore from your backup

Delete or rename the replacement `client.dll`, then rename:

```text
client.dll.original -> client.dll
```

The final path must be:

```text
Half-Life\cstrike\cl_dlls\client.dll
```

Remove `-insecure -dev -console` from the Steam launch options if you no longer
need them.

### Restore without a backup

If the backup is missing:

1. Open **Steam → Library**.
2. Right-click **Counter-Strike → Properties**.
3. Open **Installed Files**.
4. Select **Verify integrity of game files**.

Steam will download the original client again. Verification may also restore
other modified game files.

## Clone the repository

Install [Git for Windows](https://git-scm.com/download/win), open PowerShell, and
run:

```powershell
git clone https://github.com/FuryBaM/cs16-goldsrc-client.git
cd cs16-goldsrc-client
```

The default `master` branch contains the stable public version. To work with the
current development branch instead:

```powershell
git switch cs16cldll
git pull --ff-only
```

There are no Git submodules. The required HLSDK, SDL2, and VGUI headers and
import libraries are already included in the repository.

To update an existing clone later:

```powershell
git switch master
git pull --ff-only
```

## Build from source

### Requirements

- Windows 10 or Windows 11.
- **Visual Studio Community 2026** (18.x).
- The **Desktop development with C++** workload.
- **MSVC v145 C++ x64/x86 build tools**.
- A Windows 10 or Windows 11 SDK.
- Git for Windows.

The project uses C++17 and must be built as Win32/x86. A 64-bit DLL cannot be
loaded by the original Steam GoldSrc engine.

You do not need to download a separate HLSDK, SDL2 SDK, or VGUI SDK. The headers,
`SDL2.lib`, and the VGUI import library used by the project are tracked in this
repository.

### Build from Developer PowerShell

Open **Developer PowerShell for VS 2026** in the repository directory and run:

```powershell
msbuild .\cs16cldll.sln /m /p:Configuration=Release /p:Platform=x86
```

The compiled DLL will be written to:

```text
build\Release\client.dll
```

Verify the result before installing it:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\verify-client.ps1 `
  -Path .\build\Release\client.dll
```

The verification script checks that the output:

- is a PE32/x86 DLL;
- exports all 44 functions expected by Steam GoldSrc;
- does not export unexpected client entry points;
- does not import Xash3D libraries;
- imports the expected Steam GoldSrc `SDL2.dll` and `vgui.dll` libraries.

It also prints the SHA-256 hash of the built DLL.

### Build in Visual Studio

1. Open `cs16cldll.sln`.
2. Select **Release** as the configuration.
3. Select **x86** as the solution platform.
4. Select **Build → Build Solution**.
5. Run `scripts/verify-client.ps1` against `build/Release/client.dll`.

Do not select x64. The solution's `x86` platform maps to the project's Win32
configuration.

### Install your local build

After a successful build and verification:

1. Close Counter-Strike.
2. Back up `Half-Life\cstrike\cl_dlls\client.dll`.
3. Copy `build\Release\client.dll` over the game's client DLL.
4. Launch with `-insecure -dev -console`.
5. Test locally with `map de_dust2`.

## Useful console commands

```cfg
cs_vgui_team                 // open team selection
cs_vgui_class_t              // open Terrorist model selection
cs_vgui_class_ct             // open Counter-Terrorist model selection
cs_vgui_buy                  // open the buy menu
cs_vgui_hide                 // close the active VGUI panel
cs_vgui2_status              // print VGUI2 interface status
cs_vgui_reload_commandmenu   // reload commandmenu.txt
cs_test_centertext           // test the centered text HUD layer
```

Disable the new viewport and use classic text menus:

```cfg
cs_vgui_enable 0
```

Re-enable it with `cs_vgui_enable 1`.

### Radar

```cfg
cl_radar_style 1          // 0 = classic markers without the new grid
cl_radar_alpha 180        // grid and marker intensity, 40-255
cl_radar_show_location 1  // show the current location below the radar
```

### Spectator HUD

```cfg
cl_spectator_hud_modern 1 // 0 = classic spectator HUD
```

### Steam and Discord presence

Steam Rich Presence is enabled by default and displays the current map:

```cfg
cl_steam_rich_presence 1 // 0 = disabled
```

Discord RPC requires an Application ID from the
[Discord Developer Portal](https://discord.com/developers/applications):

```cfg
cl_discord_appid "123456789012345678"
cl_discord_rpc 1
```

Discord must be running on the same computer. Disable the integration with
`cl_discord_rpc 0`.

## Troubleshooting

- **MSB8020 / v145 was not found:** install Visual Studio 2026 and the MSVC v145
  x64/x86 build tools through Visual Studio Installer.
- **Windows SDK was not found:** add a Windows 10 or Windows 11 SDK under
  **Visual Studio Installer → Individual components**.
- **The DLL has the wrong architecture:** build with `Platform=x86`; never use
  x64 for the Steam GoldSrc client.
- **Windows cannot replace the DLL:** close Counter-Strike and wait for the game
  process to exit before copying the file.
- **`SDL2.dll` or `vgui.dll` is missing:** verify the Counter-Strike files in
  Steam. Do not use DLLs copied from Xash3D.
- **The VGUI2 viewport does not start:** run `cs_vgui2_status`. The client keeps a
  VGUI1 fallback when the required Steam interfaces are unavailable.
- **An unimplemented `VGUIMenu` opens:** the client falls back to the classic
  text menu. Open the menu again after the fallback is applied.

Debug builds write startup and server-message traces to:

```text
%TEMP%\cs16_goldsrc_startup.log
```

## Technical notes

The DLL is based on the standard Half-Life client lifecycle, with Counter-Strike
HUD, event, and shared-weapon code adapted from
[Velaron/cs16-client](https://github.com/Velaron/cs16-client).

The VGUI2 client dynamically opens Steam's `vgui2.dll`, validates the required
GoldSrc interfaces, and publishes `VClientVGUI001`. It deliberately avoids a
hard `vgui2.dll` import so that VGUI1 remains available as a fallback. The VGUI2
panel is registered directly as an `IClientPanel`, without linking an
incompatible static `vgui_controls.lib`.

Server-only callbacks such as `PRECACHE_MODEL`, `PRECACHE_SOUND`, and `SET_MODEL`
are isolated from the client prediction adapter. This allows shared weapon code
to run through `HUD_PostRunCmd` without calling unavailable server functions.

## Support the project

If you find this project useful and would like to support ongoing development, bug fixes, and feature updates:

- 🌐 **[Open Collective](https://opencollective.com/cs16-goldsrc-client)** — Transparent recurring or one-time contributions.
- ⚡ **[Boosty](https://boosty.to/furybam)** — Support via cards (CIS / local payment options).

## License and credits

This repository is distributed under the GNU General Public License v3.0. See
[`LICENSE.txt`](LICENSE.txt).

VGUI2 headers derived from the Source 1 SDK are stored under
`external/hl1_source_sdk` together with Valve's license and third-party notices.

Credits:

- Valve and the Half-Life SDK contributors.
- [Velaron/cs16-client](https://github.com/Velaron/cs16-client) for the
  Counter-Strike client code used as an upstream reference.
