# gunstars: Gunstar Heroes rebuilt in C.
#   make test           the function-level capture tests (out/captures/*.cap); fails on any mismatch
#                       (DRAFT=build/draft/NAME.c ...: with drafts under test, tools/disasm/draft.py)
#   make oracle         the hooked oracle core (build/oracle.so)
#   make evidence       the disassembly's evidence from every recorded session (out/evidence)
#   make disasm         the disassembly (build/disasm): byte-exact source, routines, symbols
#   make gen            the committed generated headers (src/symbols_gen.h) from the disassembly
#   make gunstar        the native game (build/gunstar, on pulseman at MDRT)
#   make windows        the game for Windows, cross-built with Zig (tools/build_windows.sh)
#   make sweep          the native game through every stage, headless (tools/native_sweep.sh)
#   make golden         the native game's golden runs, recorded (out/golden: tools/golden.py)
#   make golden-check   the build's runs against them, frame for frame, bit for bit; states too
#   make netplay-check  two headless players over a simulated network against solo runs (tools/netplay_check.py)
#   make golden-cross   the Linux release's build (Zig's Clang: make linux) against the golden runs
#   make golden-ubsan   a build with GCC's undefined behaviour sanitizer against the golden runs
#   make release        the release files in build/release (tools/package_release.sh)
CC      ?= gcc
WERROR  ?= -Werror
WARN    := -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes $(WERROR)
CFLAGS  ?= -std=c11 -O1 -g $(WARN)
SRC     := $(wildcard src/*.c)
HDR     := $(wildcard src/*.h)
TESTSRC := test/captest.c test/replay.c test/stubs.c test/ports.c test/hal_replay.c
TESTHDR := test/replay.h test/ports.h test/stubs.h
CAPS    := $(wildcard out/captures/*.cap)
ROM     := Gunstar Heroes (U) [!].gen
PY      := python3

.PHONY: FORCE all test oracle evidence disasm gen clean gunstar mdrt z80test windows sweep release golden golden-check netplay-check linux golden-cross golden-ubsan

all: build/captest                   # the default: the tests' build (not just their adapters)

DRAFT   ?=
DRAFTFLAGS := $(if $(DRAFT),-Itools/disasm -DDRAFT_TEST -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label -Wno-unused-parameter -Wno-missing-prototypes)

.PHONY: build/ports_gen.inc
build/ports_gen.inc:
	$(PY) tools/gen_ports.py $(DRAFT)

build/captest: $(SRC) $(HDR) $(TESTSRC) $(TESTHDR) build/ports_gen.inc $(DRAFT)
	@mkdir -p build
	$(CC) $(CFLAGS) $(DRAFTFLAGS) -Isrc -Itest -Ibuild -o $@ $(SRC) $(DRAFT) $(TESTSRC) -lz

# the native game: build/gunstar, on pulseman (MDRT: the submodule ext/pulseman; its release
# build with the SDL2 platform)
MDRT    ?= ext/pulseman
MDRTB   ?= build/release
NATIVE  := $(wildcard native/*.c)
MDRTLIBS := $(MDRT)/$(MDRTB)/libmdrt_sdl2.a $(MDRT)/$(MDRTB)/libmdrt.a
# (SDL2's -mwindows is left out on Windows, so the game keeps its console)
SDLLIBS := $(filter-out -mwindows,$(shell pkg-config --libs sdl2 2>/dev/null))
ifeq ($(OS),Windows_NT)
EXE     ?= .exe
# (Windows, MSYS2: the C++ runtime linked in, so the game needs only SDL2.dll beside it)
CXXLIB  ?= -static-libgcc -Wl,-Bstatic -lstdc++ -Wl,--whole-archive -lwinpthread -Wl,--no-whole-archive -Wl,-Bdynamic
endif
CXXLIB  ?= -lstdc++                  # (pulseman's ymfm core is C++)
ifeq ($(OS),Windows_NT)
NETLIB  ?= -lws2_32                  # (netplay's sockets)
endif
gunstar: build/gunstar$(EXE)
build/gunstar$(EXE): $(SRC) $(HDR) $(NATIVE) native/native.h build/ports_gen.inc build/build_id.h mdrt
	@mkdir -p build
	$(CC) $(CFLAGS) -O2 -Isrc -Ibuild -Inative -I$(MDRT)/include $(shell pkg-config --cflags sdl2 2>/dev/null) \
	    -o $@ $(SRC) $(NATIVE) $(MDRTLIBS) $(SDLLIBS) -lm $(CXXLIB) $(NETLIB)
# the build's identity, which netplay's peers compare (the same code on both): the commit (its hash,
# whatever tags it has, so a build from the tag agrees with the download), "-dirty" with changes
# not committed; written again only when it changes
build/build_id.h: FORCE
	@mkdir -p build
	@c=$$(git rev-parse HEAD 2>/dev/null | cut -c1-12); git diff --quiet HEAD 2>/dev/null || c="$$c-dirty"; \
	    id='#define GUNSTAR_BUILD "'"$${c:-unknown}"'"'; \
	    [ "$$(cat $@ 2>/dev/null)" = "$$id" ] || echo "$$id" > $@
FORCE:
# Windows from Linux: build/win/gunstar.exe and build/gunstars-windows-x64.zip, with Zig
# (tools/build_windows.sh)
windows:
	tools/build_windows.sh
sweep: build/gunstar
	tools/native_sweep.sh
golden: build/gunstar
	$(PY) tools/golden.py record
golden-check: build/gunstar
	$(PY) tools/golden.py check
netplay-check: build/gunstar
	$(PY) tools/netplay_check.py
# the same code built otherwise must play the golden runs bit for bit (netplay's peers may have
# either build): the Linux release's (Zig's Clang, tools/build_linux.sh), and one that stops at
# any undefined behaviour
linux:
	tools/build_linux.sh
golden-cross: linux
	$(PY) tools/golden.py check --bin build/linux-x64/gunstar
build/ubsan/gunstar: $(SRC) $(HDR) $(NATIVE) native/native.h build/ports_gen.inc build/build_id.h mdrt
	@mkdir -p build/ubsan
	$(CC) $(CFLAGS) -O2 -fsanitize=undefined -fno-sanitize-recover=undefined -Isrc -Ibuild -Inative -I$(MDRT)/include \
	    $(shell pkg-config --cflags sdl2 2>/dev/null) -o $@ $(SRC) $(NATIVE) $(MDRTLIBS) $(SDLLIBS) -lm $(CXXLIB) $(NETLIB)
golden-ubsan: build/ubsan/gunstar
	$(PY) tools/golden.py check --bin build/ubsan/gunstar
release:
	tools/package_release.sh
mdrt:
	@test -f $(MDRT)/Makefile || { echo "no pulseman at $(MDRT): git submodule update --init"; exit 1; }
	$(MAKE) -C $(MDRT) MODE=release B=$(MDRTB) sdl

test: build/captest
	@test -n "$(CAPS)" || { echo "no captures in out/captures (tools/hybrid/capture_all.sh records them)"; exit 1; }
	./build/captest $(CAPS)

# the Z80's sample player (src/z80.c) against the oracle's sound traces (tools/oracle/sndtrace.py,
# from power on, into out/snd)
SNDS    := $(wildcard out/snd/*.sndtrace)
build/z80test: test/z80test.c src/z80.c src/z80.h src/mem.c src/mem.h
	@mkdir -p build
	$(CC) $(CFLAGS) -O2 -Isrc -o $@ test/z80test.c src/z80.c src/mem.c
z80test: build/z80test
	@test -n "$(SNDS)" || { echo "no sound traces in out/snd (tools/oracle/sndtrace.py records them)"; exit 1; }
	./build/z80test $(SNDS)

oracle: build/oracle.so
build/oracle.so: tools/hybrid/oracle_shim.c
	$(MAKE) -s -f tools/hybrid/Makefile > /dev/null

evidence: build/oracle.so
	tools/hybrid/evidence_all.sh

disasm:
	$(PY) tools/disasm/build.py "$(ROM)"

gen: disasm
	$(PY) tools/gen_symbols.py

clean:
	rm -f build/captest build/oracle_shim.o build/oracle.so
