param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

$ErrorActionPreference = 'Stop'
$dll = (Resolve-Path $Path).Path
$bytes = [System.IO.File]::ReadAllBytes($dll)

if ($bytes.Length -lt 64) {
    throw "Not a valid PE file: $dll"
}

$peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
if ($peOffset -lt 0 -or $peOffset + 6 -gt $bytes.Length) {
    throw "Invalid PE header offset: $peOffset"
}

if ($bytes[$peOffset] -ne 0x50 -or
    $bytes[$peOffset + 1] -ne 0x45 -or
    $bytes[$peOffset + 2] -ne 0x00 -or
    $bytes[$peOffset + 3] -ne 0x00) {
    throw "Invalid PE signature in $dll"
}

$machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
if ($machine -ne 0x014c) {
    throw ('GoldSrc requires Win32/x86; PE machine is 0x{0:X4}' -f $machine)
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw 'vswhere.exe was not found'
}

$dumpbin = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -find 'VC\Tools\MSVC\**\bin\Hostx64\x86\dumpbin.exe' |
    Select-Object -Last 1

if (-not $dumpbin -or -not (Test-Path $dumpbin)) {
    throw '32-bit dumpbin.exe was not found'
}

$requiredExports = @(
    'CAM_Think',
    'CL_CameraOffset',
    'CL_CreateMove',
    'CL_IsThirdPerson',
    'CreateInterface',
    'Demo_ReadBuffer',
    'F',
    'HUD_AddEntity',
    'HUD_ChatInputPosition',
    'HUD_ConnectionlessPacket',
    'HUD_CreateEntities',
    'HUD_DirectorMessage',
    'HUD_DrawNormalTriangles',
    'HUD_DrawTransparentTriangles',
    'HUD_Frame',
    'HUD_GetHullBounds',
    'HUD_GetPlayerTeam',
    'HUD_GetStudioModelInterface',
    'HUD_GetUserEntity',
    'HUD_Init',
    'HUD_Key_Event',
    'HUD_PlayerMove',
    'HUD_PlayerMoveInit',
    'HUD_PlayerMoveTexture',
    'HUD_PostRunCmd',
    'HUD_ProcessPlayerState',
    'HUD_Redraw',
    'HUD_Reset',
    'HUD_Shutdown',
    'HUD_StudioEvent',
    'HUD_TempEntUpdate',
    'HUD_TxferLocalOverrides',
    'HUD_TxferPredictionData',
    'HUD_UpdateClientData',
    'HUD_VidInit',
    'HUD_VoiceStatus',
    'IN_Accumulate',
    'IN_ActivateMouse',
    'IN_ClearStates',
    'IN_DeactivateMouse',
    'IN_MouseEvent',
    'Initialize',
    'KB_Find',
    'V_CalcRefdef'
)

$exportText = (& $dumpbin /nologo /exports $dll) -join "`n"
$actualExports = [regex]::Matches(
    $exportText,
    '(?m)^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)(?:\s+=.*)?$'
) | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique

$missing = $requiredExports | Where-Object { $_ -notin $actualExports }
$unexpected = $actualExports | Where-Object { $_ -notin $requiredExports }
if ($missing) {
    throw "Missing GoldSrc exports: $($missing -join ', ')"
}
if ($unexpected) {
    throw "Unexpected client exports: $($unexpected -join ', ')"
}

$imports = (& $dumpbin /nologo /imports $dll) -join "`n"
foreach ($forbidden in @('xash.dll', 'mainui.dll', 'client_mobility.dll')) {
    if ($imports -match "(?im)^\s*$([regex]::Escape($forbidden))\s*$") {
        throw "Forbidden non-GoldSrc dependency: $forbidden"
    }
}
if ($imports -notmatch '(?im)^\s*SDL2\.dll\s*$') {
    throw 'Expected SDL2.dll import was not found'
}
if ($imports -notmatch '(?im)^\s*vgui\.dll\s*$') {
    throw 'Expected Steam GoldSrc vgui.dll import was not found'
}

$hash = (Get-FileHash -Algorithm SHA256 $dll).Hash
Write-Host 'Verified Steam GoldSrc Win32 client.dll'
Write-Host "Exports: $($actualExports.Count)"
Write-Host "SHA256: $hash"
