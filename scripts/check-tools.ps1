[CmdletBinding()]
param()

$Root = Split-Path -Parent $PSScriptRoot
$VenvPython = Join-Path $Root ".venv/Scripts/python.exe"

function Test-Command {
    param([string]$Name)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) {
        [pscustomobject]@{ Tool = $Name; Status = "found"; Detail = $cmd.Source }
    } else {
        [pscustomobject]@{ Tool = $Name; Status = "missing"; Detail = "" }
    }
}

function Test-PathStatus {
    param(
        [string]$Name,
        [string]$Path
    )
    if (Test-Path $Path) {
        [pscustomobject]@{ Tool = $Name; Status = "found"; Detail = $Path }
    } else {
        [pscustomobject]@{ Tool = $Name; Status = "missing"; Detail = $Path }
    }
}

$checks = @()
$checks += Test-Command git
$checks += Test-Command python
$checks += Test-Command docker
$checks += Test-Command cmake
$checks += Test-Command ninja
$checks += Test-Command make
$checks += Test-Command arm-none-eabi-as
$checks += Test-Command arm-none-eabi-gcc
$checks += Test-Command arm-none-eabi-objcopy
$checks += Test-Command mgba
$checks += Test-Command mgba-qt
$checks += Test-Command ghidraRun
$checks += Test-PathStatus "Python venv" $VenvPython
$checks += Test-PathStatus "agbcc binary" (Join-Path $Root "tools/agbcc/bin/agbcc")
$checks += Test-PathStatus "old_agbcc binary" (Join-Path $Root "tools/agbcc/bin/old_agbcc")
$checks += Test-PathStatus "agbcc_arm binary" (Join-Path $Root "tools/agbcc/bin/agbcc_arm")
$checks += Test-PathStatus "gbafix binary" (Join-Path $Root "tools/gba-tools/bin/gbafix")
$checks += Test-PathStatus "gbalzss binary" (Join-Path $Root "tools/gba-tools/bin/gbalzss")
$checks += Test-PathStatus "agbcc source" (Join-Path $Root "tools/src/agbcc")
$checks += Test-PathStatus "gba-tools source" (Join-Path $Root "tools/src/gba-tools")

$checks | Format-Table -AutoSize

if (Test-Path $VenvPython) {
    Write-Host ""
    Write-Host "Python tool imports:"
$ImportCheck = @'
import importlib.util
for name in ["capstone", "elftools", "intervaltree", "luvdis"]:
    print(f"{name}: {'found' if importlib.util.find_spec(name) else 'missing'}")
'@
    $ImportCheck | & $VenvPython -
}
