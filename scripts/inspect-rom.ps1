param(
    [string]$RomPath = "rom\Advance Wars (USA) (Rev 1).gba",
    [string]$BuildDir = "build\native"
)

$ErrorActionPreference = "Stop"

$exe = Join-Path $BuildDir "runtime\advance-wars-native.exe"
if (-not (Test-Path $exe)) {
    cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

& $exe $RomPath
exit $LASTEXITCODE
