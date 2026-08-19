#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_SRC="$ROOT/tools/src"
VENV="$ROOT/.venv"
BUILD_DOCKER=0

for arg in "$@"; do
  case "$arg" in
    --build-docker) BUILD_DOCKER=1 ;;
    *) echo "Unknown argument: $arg" >&2; exit 2 ;;
  esac
done

require() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Missing required command: $1" >&2
    exit 1
  }
}

sync_repo() {
  local name="$1"
  local url="$2"
  local path="$3"

  if [ -d "$path/.git" ]; then
    echo "==> Updating $name"
    git -C "$path" fetch --tags --prune
    git -C "$path" pull --ff-only
  else
    if [ -e "$path" ]; then
      echo "$path exists but is not a Git repository." >&2
      exit 1
    fi
    echo "==> Cloning $name"
    git clone "$url" "$path"
  fi
}

cd "$ROOT"

echo "==> Checking host basics"
require git
require python3

echo "==> Creating/updating local Python tool venv"
if [ ! -d "$VENV" ]; then
  python3 -m venv "$VENV"
fi
"$VENV/bin/python" -m pip install --upgrade pip
"$VENV/bin/python" -m pip install -r "$ROOT/requirements-tools.txt"

mkdir -p "$TOOLS_SRC"
sync_repo "agbcc" "https://github.com/pret/agbcc.git" "$TOOLS_SRC/agbcc"
sync_repo "gba-tools" "https://github.com/devkitPro/gba-tools.git" "$TOOLS_SRC/gba-tools"

if [ "$BUILD_DOCKER" -eq 1 ]; then
  echo "==> Building Docker toolchain image"
  require docker
  docker build -f "$ROOT/tools/Dockerfile.gba" -t advance-wars-recomp-gba-tools "$ROOT"
fi

echo "==> Bootstrap complete"
