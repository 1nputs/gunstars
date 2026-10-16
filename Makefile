# gunstar: Gunstar Heroes rebuilt in C.
#   make test           the function-level capture tests (out/captures/*.cap); fails on any mismatch
#   make oracle         the hooked oracle core (build/oracle.so)
#   make evidence       the disassembly's evidence from every recorded session (out/evidence)
#   make disasm         the disassembly (build/disasm): byte-exact source, routines, symbols
#   make gen            the committed generated headers (src/symbols_gen.h) from the disassembly
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

.PHONY: test oracle evidence disasm gen clean

build/captest: $(SRC) $(HDR) $(TESTSRC) $(TESTHDR)
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc -Itest -o $@ $(SRC) $(TESTSRC)

test: build/captest
	@test -n "$(CAPS)" || { echo "no captures in out/captures (tools/hybrid/capture_all.sh records them)"; exit 1; }
	./build/captest $(CAPS)

oracle: build/oracle.so
build/oracle.so: tools/hybrid/oracle_shim.c
	$(MAKE) -f tools/hybrid/Makefile

evidence: build/oracle.so
	tools/hybrid/evidence_all.sh

disasm:
	$(PY) tools/disasm/build.py "$(ROM)"

gen: disasm
	$(PY) tools/gen_symbols.py

clean:
	rm -f build/captest build/oracle_shim.o build/oracle.so
