[CmdletBinding()]
param(
    [switch]$BuildDocker,
    [switch]$SkipPython,
    [switch]$SkipSources
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$ToolsSrc = Join-Path $Root "tools/src"
$Venv = Join-Path $Root ".venv"

function Write-Step {
    param([string]$Message)
    Write-Host "==> $Message"
}

function Require-Command {
    param([string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Missing required command: $Name"
    }
}

function Sync-GitRepo {
    param(
        [string]$Name,
        [string]$Url,
        [string]$Path
    )

    if (Test-Path (Join-Path $Path ".git")) {
        Write-Step "Updating $Name"
        git -C $Path fetch --tags --prune
        git -C $Path pull --ff-only
        return
    }

    if (Test-Path $Path) {
        throw "$Path exists but is not a Git repository. Move it aside or remove it, then rerun."
    }

    Write-Step "Cloning $Name"
    git clone $Url $Path
}

Set-Location $Root

Write-Step "Checking host basics"
Require-Command git
Require-Command python

if (-not $SkipPython) {
    Write-Step "Creating/updating local Python tool venv"
    if (-not (Test-Path $Venv)) {
        python -m venv $Venv
    }

    $VenvPython = Join-Path $Venv "Scripts/python.exe"
    & $VenvPython -m pip install --upgrade pip
    & $VenvPython -m pip install -r (Join-Path $Root "requirements-tools.txt")
}

if (-not $SkipSources) {
    New-Item -ItemType Directory -Force -Path $ToolsSrc | Out-Null
    Sync-GitRepo "agbcc" "https://github.com/pret/agbcc.git" (Join-Path $ToolsSrc "agbcc")
    Sync-GitRepo "gba-tools" "https://github.com/devkitPro/gba-tools.git" (Join-Path $ToolsSrc "gba-tools")
}

if ($BuildDocker) {
    Write-Step "Building Docker toolchain image"
    Require-Command docker
    docker build -f (Join-Path $Root "tools/Dockerfile.gba") -t advance-wars-recomp-gba-tools $Root
}

Write-Step "Bootstrap complete"
Write-Host "Run .\scripts\check-tools.ps1 for a tool availability report."
