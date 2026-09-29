# DOS-GL: OpenGL 1.1 subset for Matrox G400/G450/G200 under DJGPP (PRD.md).
#
#   make                  libGL.a and the examples (DJGPP)
#   make tests-host       host unit tests (Linux gcc)
#   make loopa TEST=hello [CARD=g450] [ARGS=--fail]    run an example in 86Box
#   make quake / loopa-quake GAME=quake2   the Quake ports (tools/quake)
#   make sync-hal         refresh third_party/mgahal from MGA-Glide
#   make check-hal        verify the vendored copy against its MANIFEST
include config.mk
-include config.local.mk
.SECONDEXPANSION:

MGAHAL   := third_party/mgahal
BUILD_ID := $(shell git describe --always --dirty 2>/dev/null || echo unknown)
Q ?= @

DJENV := env LD_LIBRARY_PATH=$(DJGPP_PREFIX)/hostlib
DJCC  := $(DJENV) $(DJGPP_PREFIX)/bin/i586-pc-msdosdjgpp-gcc
DJAR  := $(DJENV) $(DJGPP_PREFIX)/bin/i586-pc-msdosdjgpp-ar
DJ_CFLAGS := -std=gnu99 -O2 -march=i586 -Wall -Wextra -Werror -Iinclude -I$(MGAHAL)/hal/include \
             -DMGA_DJGPP=1 -DDGL_BUILD_ID='"$(BUILD_ID)"'
DJ_TESTFLAGS := -I$(MGAHAL)/tests/shim -DHX_BUILD_ID='"$(BUILD_ID)"'

# The shared HAL (PRD D15) goes into libGL.a so consumers link only -lGL.
HAL_SRCS := $(addprefix $(MGAHAL)/hal/,src/debug/serial.c src/pci.c src/chip.c src/vbe.c src/fifo.c \
              src/engine.c src/dac.c src/texhw.c src/present.c src/debug/regtrace.c src/setup/trap.c port/djgpp.c)
DGL_SRCS := $(wildcard src/dgl/*.c) $(wildcard src/gl/*.c)
LIB := build/lib/libGL.a

.PHONY: all lib examples tests-host loopa conform conform-dos conform-host classicube loopa-classicube quake loopa-quake setup-djgpp setup-ow dostools 86box sync-hal check-hal clean help
all: lib examples

build/djgpp/%.o: %.c
	@mkdir -p $(dir $@)
	$(Q)echo "  DJCC    $<"
	$(Q)$(DJCC) $(DJ_CFLAGS) -MMD -c -o $@ $<
-include $(shell find build/djgpp -name '*.d' 2>/dev/null)

# The version string carries the build ID: rebuild it whenever the ID changes
# (a new commit, or the tree turning dirty or clean), not only when the source does.
build/build_id: FORCE
	@mkdir -p $(dir $@)
	@echo '$(BUILD_ID)' | cmp -s - $@ || echo '$(BUILD_ID)' > $@
build/djgpp/src/dgl/version.o: build/build_id
.PHONY: FORCE

# Generated: stubs for the GL 1.1 functions not implemented, and the
# dglGetProcAddress table (tools/gen_stubs.py; files rewritten only on change).
GEN_SRCS := build/gen/stubs.c build/gen/procs.c
$(GEN_SRCS) &: tools/gen_stubs.py src/gl/gl11.api $(DGL_SRCS)
	$(Q)python3 tools/gen_stubs.py src/gl src/dgl build/gen

$(LIB): $(DGL_SRCS:%.c=build/djgpp/%.o) $(GEN_SRCS:%.c=build/djgpp/%.o) $(HAL_SRCS:%.c=build/djgpp/%.o)
	@mkdir -p $(dir $@)
	$(Q)echo "  AR      $@"
	$(Q)rm -f $@ && $(DJAR) rcs $@ $^
lib: $(LIB)

# Examples: one directory each, linked with the guest test shim (HX- lines).
EXAMPLES := hello probe clear tri cube texcube g4exp resgl
build/exe/%.EXE: $(LIB) $(MGAHAL)/tests/shim/hx.c
	@mkdir -p $(dir $@)
	$(Q)echo "  DJLD    $@"
	$(Q)$(DJCC) $(DJ_CFLAGS) $(DJ_TESTFLAGS) -o $@ $(wildcard examples/$(shell echo $* | tr A-Z a-z)/*.c) \
	  $(MGAHAL)/tests/shim/hx.c $(LIB) -lm
	@if [ -e "$(@:.EXE=.exe)" ] && ! [ "$(@:.EXE=.exe)" -ef "$@" ]; then rm -f "$(@:.EXE=.exe)"; fi
build/exe/HELLO.EXE: $(wildcard examples/hello/*.c)
build/exe/PROBE.EXE: $(wildcard examples/probe/*.c) src/dgl/dgl.h
build/exe/CLEAR.EXE: $(wildcard examples/clear/*.c) src/dgl/dgl.h
build/exe/TRI.EXE: $(wildcard examples/tri/*.c) src/dgl/dgl.h
build/exe/CUBE.EXE: $(wildcard examples/cube/*.c)
build/exe/TEXCUBE.EXE: $(wildcard examples/texcube/*.c)
build/exe/G4EXP.EXE: $(wildcard examples/g4exp/*.c) src/dgl/dgl.h src/gl/gl_tex.h
build/exe/RESGL.EXE: $(wildcard examples/resgl/*.c)
examples: $(foreach e,$(EXAMPLES),build/exe/$(shell echo $(e) | tr a-z A-Z).EXE)

# Conformance tests (tests/conform, D16): DOS builds against libGL.a and
# host builds against Mesa OSMesa (the references; built in the dev container).
CONFORM := $(sort $(foreach f,$(wildcard tests/conform/t[0-9]*_*.c),$(firstword $(subst _, ,$(notdir $(f))))))
build/exe/conform/%.EXE: $(LIB) tests/conform/ct_dos.c tests/conform/ct.h tests/conform/ct_tex.h \
                         $(MGAHAL)/tests/shim/hx.c $$(wildcard tests/conform/$$(shell echo $$* | tr A-Z a-z)_*.c)
	@mkdir -p $(dir $@)
	$(Q)echo "  DJLD    $@"
	$(Q)$(DJCC) $(DJ_CFLAGS) $(DJ_TESTFLAGS) -DCT_NAME='"$*"' -o $@ $(wildcard tests/conform/$(shell echo $* | tr A-Z a-z)_*.c) \
	  tests/conform/ct_dos.c $(MGAHAL)/tests/shim/hx.c $(LIB) -lm
	@if [ -e "$(@:.EXE=.exe)" ] && ! [ "$(@:.EXE=.exe)" -ef "$@" ]; then rm -f "$(@:.EXE=.exe)"; fi
build/host/conform/%: $$(wildcard tests/conform/$$*_*.c) tests/conform/ct_host.c tests/conform/ct.h tests/conform/ct_tex.h
	@mkdir -p $(dir $@)
	$(Q)$(HOST_CC) -std=gnu99 -O1 -Wall -Wextra -Werror -o $@ $(wildcard tests/conform/$*_*.c) tests/conform/ct_host.c \
	  -lOSMesa -lm
conform-dos: $(foreach t,$(CONFORM),build/exe/conform/$(shell echo $(t) | tr a-z A-Z).EXE)
conform-host: $(foreach t,$(CONFORM),build/host/conform/$(t))
conform: conform-dos dostools
	$(MGAHAL)/tools/dev sh -c '$(MAKE) conform-host && python3 tools/conform/run.py --card $(CARD) \
	  $(if $(PRE),--pre "$(PRE)") $(TESTS)'

# ClassiCube (M5): build against libGL.a, and run it in 86Box with the
# procedural test texture pack (tools/classicube/mkpack.py): singleplayer,
# DGL_EXIT_AFTER frames, DGL_STATS lines, console screenshots.
CC_FRAMES ?= 300
classicube: lib
	tools/classicube/build.sh
build/cc/default.zip: tools/classicube/mkpack.py
	@mkdir -p $(dir $@)
	python3 tools/classicube/mkpack.py $@
loopa-classicube: classicube build/cc/default.zip dostools
	$(MGAHAL)/tools/dev python3 $(MGAHAL)/tools/loopa/run.py --name classicube --exe build/cc/CCDOS.EXE \
	  --card $(CARD) --out $(CURDIR)/out/classicube-$(CARD) --args="--singleplayer" \
	  --file "build/cc/default.zip=/TEST/TEXPACKS/DEFAULT.ZIP" \
	  --pre "SET DGL_EXIT_AFTER=$(CC_FRAMES)" --pre "SET DGL_STATS=1" \
	  --timeout 1500 --idle 300 --shots 60,120,180

# The Quake ports (tools/quake): the pinned forks built against libGL.a, run
# in 86Box on the owner's game data (tools/quake/fixtures.py; never committed).
#   make loopa-quake GAME=quake|lq|quake2 [CARD=g450]
quake: lib
	tools/quake/build.sh
loopa-quake: quake dostools
	tools/quake/run.sh $(GAME) $(CARD)

# Host unit tests.
HOST_CFLAGS := -std=gnu11 -O1 -g -Wall -Wextra -Werror -Iinclude
build/host/test_gl_h_abi: tests/unit/test_gl_h_abi.c include/GL/gl.h include/GL/glext.h
	@mkdir -p $(dir $@)
	$(Q)$(HOST_CC) $(HOST_CFLAGS) -Ithird_party/classicube -o $@ $<
# Unit tests: tests/unit/test_<name>.c + UNIT_<name> sources, host gcc.
GL_SRCS := $(wildcard src/gl/*.c)
UNIT_TESTS := matrix clip assembly texconv vram lists
UNIT_lists := src/gl/lists.c src/gl/vertex.c src/gl/matrix.c src/gl/state.c src/gl/raster.c
UNIT_texconv := src/gl/texconv.c
UNIT_vram := src/gl/vram.c
UNIT_matrix := src/gl/matrix.c src/gl/state.c
UNIT_clip := src/gl/clip.c
UNIT_assembly := src/gl/vertex.c src/gl/matrix.c src/gl/state.c src/gl/raster.c
build/host/test_%: tests/unit/test_%.c tests/unit/unit.c tests/unit/unit.h $$(UNIT_$$*) $(wildcard src/gl/*.h)
	@mkdir -p $(dir $@)
	$(Q)$(HOST_CC) $(HOST_CFLAGS) -o $@ $< tests/unit/unit.c $(UNIT_$*) -lm
tests-host: build/host/test_gl_h_abi $(UNIT_TESTS:%=build/host/test_%)
	@set -e; for t in $^; do echo "== $$t"; $$t; done

# Loop A: the shared harness (86Box with the local patches), run in the
# dev container. Outputs in out/<TEST>/.
TEST ?= hello
CARD ?= g450
loopa: examples dostools
	$(MGAHAL)/tools/dev python3 $(MGAHAL)/tools/loopa/run.py --name $(TEST) \
	  --exe build/exe/$(shell echo $(TEST) | tr a-z A-Z).EXE --card $(CARD) --out $(CURDIR)/out/$(TEST) \
	  $(if $(ARGS),--args="$(ARGS)")

# Toolchain and emulator come from the vendored harness (pinned in its
# tools/setup/versions.mk; cached in ~/.cache/mga-glide, shared with MGA-Glide).
setup-djgpp:
	$(MAKE) -C $(MGAHAL) setup-djgpp DJGPP_PREFIX=$(DJGPP_PREFIX)
setup-ow:
	$(MAKE) -C $(MGAHAL) setup-ow
# The 16-bit DOS helpers Loop A and the bench put in C:\HX (built with Open Watcom).
dostools:
	$(MAKE) -C $(MGAHAL) dostools
86box:
	$(MAKE) -C $(MGAHAL) 86box

sync-hal:
	tools/sync-hal.sh $(MGA_GLIDE)
check-hal:
	tools/sync-hal.sh --check

clean:
	rm -rf build out

help:
	@sed -n '3,8p' Makefile
