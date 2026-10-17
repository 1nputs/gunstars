# gunstar: Gunstar Heroes rebuilt in C.
#   make test           the function-level capture tests (out/captures/*.cap); fails on any mismatch
#                       (DRAFT=build/draft/NAME.c ...: with drafts under test, tools/disasm/draft.py)
#   make oracle         the hooked oracle core (build/oracle.so)
#   make evidence       the disassembly's evidence from every recorded session (out/evidence)
#   make disasm         the disassembly (build/disasm): byte-exact source, routines, symbols
#   make gen            the committed generated headers (src/symbols_gen.h) from the disassembly
#   make gunstar        the native game (build/gunstar, on pulseman at MDRT)
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

.PHONY: all test oracle evidence disasm gen clean gunstar mdrt

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
SDLLIBS := $(filter-out -mwindows,$(shell pkg-config --libs sdl2 2>/dev/null))
gunstar: build/gunstar
build/gunstar: $(SRC) $(HDR) $(NATIVE) native/native.h build/ports_gen.inc mdrt
	@mkdir -p build
	$(CC) $(CFLAGS) -O2 -Isrc -Ibuild -Inative -I$(MDRT)/include $(shell pkg-config --cflags sdl2 2>/dev/null) \
	    -o $@ $(SRC) $(NATIVE) $(MDRTLIBS) $(SDLLIBS) -lm -lstdc++
mdrt:
	@test -f $(MDRT)/Makefile || { echo "no pulseman at $(MDRT): git submodule update --init"; exit 1; }
	$(MAKE) -C $(MDRT) MODE=release B=$(MDRTB) sdl

test: build/captest
	@test -n "$(CAPS)" || { echo "no captures in out/captures (tools/hybrid/capture_all.sh records them)"; exit 1; }
	./build/captest $(CAPS)

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
