[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$AgbccSrc = Join-Path $Root "tools/src/agbcc"

if (-not (Test-Path (Join-Path $AgbccSrc "build.sh"))) {
    throw "Missing agbcc source. Run .\scripts\bootstrap-tools.ps1 first."
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
    -w /work/tools/src/agbcc `
    advance-wars-recomp-gba-tools `
    bash -lc "chmod +x build.sh install.sh && ./build.sh && ./install.sh /work"
