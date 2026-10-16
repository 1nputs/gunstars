#!/usr/bin/env bash
# Fetch and build the reference material this project builds on. Nothing here is committed.
#   ref/gpgx   Genesis Plus GX at the commit the oracle was checked with, patched (tools/patches)
#              and built as a libretro core with HOOK_CPU: the oracle (a development tool only;
#              its licence is non-commercial, it is never shipped)
#   ref/vasm-src  vasm (m68k, Motorola syntax): assembles the disassembly to check it
#   ref/tas    TASVideos' two-player movie (Samsara, 3274M): its menu inputs start the sessions
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p ref
GPGX_COMMIT=49c5847
if [ ! -d ref/gpgx ]; then
  git clone https://github.com/ekeeke/Genesis-Plus-GX.git ref/gpgx
  git -C ref/gpgx checkout -q "$GPGX_COMMIT"
fi
tools/patches/gpgx-cpuhook-extern.sh ref/gpgx
tools/patches/gpgx-evidence-hooks.sh ref/gpgx
if [ ! -f ref/gpgx/core/z80/z80.o ] || [ tools/patches/gpgx-evidence-hooks.sh -nt ref/gpgx/core/z80/z80.o ]; then
  touch ref/gpgx/core/z80/z80.c ref/gpgx/core/m68k/m68kcpu.c ref/gpgx/core/m68k/s68kcpu.c
  make -C ref/gpgx -f Makefile.libretro HOOK_CPU=1 -j"$(nproc)" > ref/gpgx/hook_build.log 2>&1
fi
if [ ! -x ref/vasm-src/vasm/vasmm68k_mot ]; then
  mkdir -p ref/vasm-src && curl -sL -o ref/vasm-src/vasm.tar.gz http://sun.hasenbraten.de/vasm/release/vasm.tar.gz
  tar xzf ref/vasm-src/vasm.tar.gz -C ref/vasm-src && tools/patches/vasm-build.sh ref/vasm-src/vasm
fi
if [ ! -f ref/tas/samsara-gunstarheroes.bk2 ]; then
  mkdir -p ref/tas
  curl -sL -o ref/tas/3274M.zip "https://tasvideos.org/3274M?handler=Download"
  unzip -p ref/tas/3274M.zip > ref/tas/samsara-gunstarheroes.bk2
fi
echo "refs ready"
