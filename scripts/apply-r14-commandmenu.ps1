$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $repoRoot

$patch = Join-Path $repoRoot 'patches/r14-commandmenu-unicode.patch'
if (-not (Test-Path $patch)) {
    throw "Patch not found: $patch"
}

git apply --check $patch
git apply $patch

Write-Host 'R14 command menu source patch applied.'
Write-Host 'Build the Win32 GoldSrc client target and verify all 44 exports.'
