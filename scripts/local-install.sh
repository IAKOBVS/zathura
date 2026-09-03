#!/bin/sh
# Local install to $HOME/.local (for development without root)
# Usage: ./scripts/local-install.sh [--prefix $HOME/.local] [--clean]
# Idempotent: reuses existing build/ if present, otherwise runs meson setup.
set -eu

PREFIX="${1:-$HOME/.local}"
# allow --prefix arg
if [ "${1:-}" = "--prefix" ]; then
  PREFIX="${2:-$HOME/.local}"
  shift 2 || true
fi
CLEAN=0
if [ "${1:-}" = "--clean" ]; then
  CLEAN=1
fi

# shell completions try to install to /usr/share/bash-completion by default
# which fails without root; disable for clean local install unless explicitly enabled
COMPLETION_OPT="-Dshell-completions=disabled"
# if user wants completions, they can pass --with-completions
if [ "${1:-}" = "--with-completions" ]; then
  COMPLETION_OPT=""
fi

echo "==> prefix: $PREFIX"
echo "==> completions: ${COMPLETION_OPT:-enabled}"

if [ "$CLEAN" = "1" ]; then
  echo "==> cleaning build/"
  rm -rf build
fi

if [ ! -d build ]; then
  echo "==> meson setup build --prefix $PREFIX $COMPLETION_OPT"
  # shellcheck disable=SC2086
  meson setup build --prefix "$PREFIX" $COMPLETION_OPT
else
  echo "==> meson configure build --prefix $PREFIX $COMPLETION_OPT"
  # shellcheck disable=SC2086
  meson configure build --prefix "$PREFIX" $COMPLETION_OPT || {
    echo "==> configure failed, trying fresh setup"
    rm -rf build
    meson setup build --prefix "$PREFIX" $COMPLETION_OPT
  }
fi

echo "==> ninja -C build"
ninja -C build

echo "==> ninja -C build install"
if ! ninja -C build install; then
  echo "!! install failed (likely bash-completion to /usr) — retrying with -Dshell-completions=disabled" >&2
  meson configure build -Dshell-completions=disabled
  ninja -C build install
fi

echo "==> installed to $PREFIX/bin/zathura*"
ls -lh "$PREFIX/bin/zathura"* || true
echo "==> manpages: $PREFIX/share/man/man1/zathura.1 $PREFIX/share/man/man5/zathurarc.5"
"$PREFIX/bin/zathura" --version || true
