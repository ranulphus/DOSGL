# Matrox HAL: shared by MGA-Glide (Open Watcom, DOS/4GW) and DOS-GL (DJGPP).
include tools/setup/versions.mk
DJGPP_PREFIX ?= $(HOME)/.local/opt/djgpp-gcc1220
WATCOM ?= $(HOME)/.local/opt/watcom-20260901
export WATCOM DJGPP_PREFIX DJGPP_URL DJGPP_SHA256 CWSDPMI_URL CWSDPMI_SHA256 OW_URL OW_SHA256
Q ?= @

HAL_COMMON := hal/src/debug/serial.c hal/src/debug/regtrace.c hal/src/pci.c hal/src/chip.c hal/src/vbe.c hal/src/fifo.c hal/src/engine.c hal/src/dac.c hal/src/texhw.c hal/src/present.c hal/src/setup/trap.c

DJENV := env LD_LIBRARY_PATH=$(DJGPP_PREFIX)/hostlib
DJCC := $(DJENV) $(DJGPP_PREFIX)/bin/i586-pc-msdosdjgpp-gcc
DJAR := $(DJENV) $(DJGPP_PREFIX)/bin/i586-pc-msdosdjgpp-ar
DJ_CFLAGS := -std=gnu99 -O2 -march=i586 -Wall -Wextra -Werror -Ihal/include -DMGA_DJGPP=1
HOST_CFLAGS := -std=gnu99 -O1 -g -Wall -Wextra -Werror -Ihal/include -DMGA_HOST=1 -fno-strict-aliasing
OWBIN := $(WATCOM)/binl64
OWENV := env WATCOM=$(WATCOM) PATH=$(OWBIN):$(PATH) INCLUDE=$(WATCOM)/h
WCC := $(OWENV) $(OWBIN)/wcc386
OW_CFLAGS := -bt=dos -mf -3s -fp5 -fpi87 -zri -ei -j -zastd=c99 -zq -we -wx -i=hal/include -dMGA_OW=1 -oxt

.PHONY: all hal-djgpp hal-host hal-ow tests-host smoke-djgpp dostests-djgpp dostools setup-djgpp setup-ow 86box clean
all: hal-djgpp hal-host tests-host

build/djgpp/%.o: %.c
	@mkdir -p $(dir $@)
	$(Q)$(DJCC) $(DJ_CFLAGS) -c -o $@ $<
build/djgpp/libmgahal.a: $(HAL_COMMON:%.c=build/djgpp/%.o) build/djgpp/hal/port/djgpp.o
	$(Q)$(DJAR) rcs $@ $^
hal-djgpp: build/djgpp/libmgahal.a

build/djgpp/SMOKE.EXE: tests/hal/smoke.c build/djgpp/libmgahal.a
	$(Q)$(DJCC) $(DJ_CFLAGS) -o $@ $^
smoke-djgpp: build/djgpp/SMOKE.EXE

# Guest shim programs with DJGPP (Loop A and bench jobs run them with CWSDPMI).
build/djgpp/HELLO.EXE: tests/shim/hello.c tests/shim/hx.c build/djgpp/libmgahal.a
	$(Q)$(DJCC) $(DJ_CFLAGS) -Itests/shim -DHX_BUILD_ID='"mgahal"' -o $@ $^
build/djgpp/PROBE.EXE: tests/hal/probe.c tests/shim/hx.c build/djgpp/libmgahal.a
	$(Q)$(DJCC) $(DJ_CFLAGS) -Itests/shim -DHX_BUILD_ID='"mgahal"' -o $@ $^
# The emulator's DPMI stack check (docs/loops.md): must PASS in Loop A.
build/djgpp/STACKPG.EXE: tests/shim/stackpg.c tests/shim/hx.c build/djgpp/libmgahal.a
	$(Q)$(DJCC) $(DJ_CFLAGS) -Itests/shim -DHX_BUILD_ID='"mgahal"' -o $@ $^
dostests-djgpp: build/djgpp/HELLO.EXE build/djgpp/PROBE.EXE build/djgpp/STACKPG.EXE

build/host/%.o: %.c
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(HOST_CFLAGS) -c -o $@ $<
build/host/libmgahal.a: $(HAL_COMMON:%.c=build/host/%.o) build/host/hal/port/host.o
	$(Q)ar rcs $@ $^
hal-host: build/host/libmgahal.a

build/host/test_trap: tests/unit/test_trap.c tests/unit/unit.c tests/unit/refrast.c hal/src/setup/trap.c
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(HOST_CFLAGS) -o $@ $^ -lm
tests-host: build/host/test_trap
	build/host/test_trap

build/ow/%.obj: %.c
	@mkdir -p $(dir $@)
	$(Q)$(WCC) $(OW_CFLAGS) -fo=$@ $<
hal-ow: $(HAL_COMMON:%.c=build/ow/%.obj) build/ow/hal/port/ow_dos4g.obj

# 16-bit DOS helpers for Loop A and the bench (SERSAY, UTEXIT, WAITSEC, REBOOT,
# VMODE), built with Open Watcom's 16-bit compiler (make setup-ow).
DOS_TOOLS := UTEXIT SERSAY WAITSEC REBOOT VMODE
build/ow/dos/%.COM:
	@mkdir -p build/ow/dos/obj16
	$(Q)$(OWENV) $(OWBIN)/wcc -bt=dos -ms -0 -os -zq -we -fo=build/ow/dos/obj16/$*.obj $<
	$(Q)$(OWENV) $(OWBIN)/wlink option quiet system com name $@ file build/ow/dos/obj16/$*.obj
build/ow/dos/UTEXIT.COM: tools/dos/utexit.c
build/ow/dos/SERSAY.COM: tools/dos/sersay.c
build/ow/dos/WAITSEC.COM: tools/dos/waitsec.c
build/ow/dos/REBOOT.COM: tools/dos/reboot.c
build/ow/dos/VMODE.COM: tools/dos/vmode.c
dostools: $(DOS_TOOLS:%=build/ow/dos/%.COM)

setup-djgpp:
	tools/setup/setup-djgpp.sh
setup-ow:
	tools/setup/setup-ow.sh
86box:
	tools/dev tools/86box/build.sh
clean:
	rm -rf build
