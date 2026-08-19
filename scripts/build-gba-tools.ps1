[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$GbaToolsSrc = Join-Path $Root "tools/src/gba-tools"

if (-not (Test-Path (Join-Path $GbaToolsSrc "autogen.sh"))) {
    throw "Missing gba-tools source. Run .\scripts\bootstrap-tools.ps1 first."
}

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    throw "Docker is required for this wrapper."
}

docker image inspect advance-wars-recomp-gba-tools *> $null
if ($LASTEXITCODE -ne 0) {
    throw "Missing Docker image advance-wars-recomp-gba-tools. Run .\scripts\bootstrap-tools.ps1 -BuildDocker first."
}

$RepoMount = $Root -replace "\\", "/"
if ($RepoMount -match "^([A-Za-z]):") {
    $drive = $Matches[1].ToLowerInvariant()
    $RepoMount = "/$drive" + $RepoMount.Substring(2)
}

docker run --rm `
    -v "${RepoMount}:/work" `
    -w /work/tools/src/gba-tools `
    advance-wars-recomp-gba-tools `
    bash -lc "set -e; sed -i 's/\r$//' autogen.sh configure.ac Makefile.am m4/*.m4 src/*.c src/*.cpp src/*.h; grep -q '<cstdint>' src/gbalzss.cpp || sed -i '/#include <cstddef>/a #include <cstdint>' src/gbalzss.cpp; chmod +x autogen.sh && ./autogen.sh && ./configure --prefix=/work/tools/gba-tools && make -j`$(nproc) && make install"
