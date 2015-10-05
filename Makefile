# basic setup

DISTNAME  := pixelserv
CC        := gcc
OPTS      := -DDROP_ROOT -DIF_MODE
SRCS      := util.c socket_handler.c pixelserv.c

# debug flags
CFLAGS_D  := $(CFLAGS) -g -Wall
LDFLAGS_D := $(LDFLAGS)

# performance flags
CFLAGS_P  := $(CFLAGS) -O3 -s -Wall -ffunction-sections -fdata-sections -fno-strict-aliasing
LDFLAGS_P := $(LDFLAGS) -Wl,--gc-sections

# aggressive strip command
# note that this breaks the x86 build on x86-64 for some reason
#STRIP     := strip -s -R .note -R .comment -R .gnu.version -R .gnu.version_r
# Less aggressive, works for RPi
STRIP     := strip -s -R .note -R .comment -R .gnu.version

# agressive UPX compress
UPX       := upx -9

# packaging macros
PFILES    := LICENSE README.md
PVERSION  := $(shell grep VERSION util.h | awk '{print $$NF}' | sed 's|\"||g')
PCMD      := zip

# x86 environment
CC32      := $(CC) -m32

# x86_64 environment
CC64      := $(CC) -m64

# MIPS environment
# I set my path ahead of time, so I've commented out the path command macro below
MIPSTOOLS := /opt/brcm/hndtools-mipsel-uclibc/bin:/opt/brcm/hndtools-mipsel-linux/bin
#MIPSPATH  := PATH=$(MIPSTOOLS):$(PATH)
MIPSREFIX := mipsel-uclibc-
MIPSCC    := $(MIPSREFIX)$(CC)
MIPSSTRIP := $(MIPSREFIX)$(STRIP)

# ARM environment
ARMTOOLS  := /usr/local/x-tools/arm-unknown-linux-gnueabihf/bin
#ARMPATH   := PATH=$(ARMTOOLS):$(PATH)
ARMPREFIX := arm-unknown-linux-gnueabihf-
ARMCC     := $(ARMPREFIX)$(CC)
ARMSTRIP  := $(ARMPREFIX)$(STRIP)

# RPi environment
RPITOOLS  := /usr/bin
#RPIPATH  := PATH=$(RPITOOLS):$(PATH)
RPIPREFIX := arm-linux-gnueabihf-
RPICC     := $(RPIPREVIX)$(CC)
RPIOPTS   := -lrt
RPISTRIP  := $(RPIPREFIX)$(STRIP)

# tomatoware environment uses basic setup options because it compiles native

# targets - notes:
# - for each platform, there should be 4 versions: dynamic performance, dynamic debug, static performance, static debug
# - static is not built for x86* targets because it causes glibc-related complaints
# - mips version could be K24 or K26 depending on environment
# - tomatoware is not included in the 'all' target, because it's for compiling natively on a router

.PHONY: all clean distclean printver x86 x86_x64 mips arm tomatoware RPi

all: x86 x86_64 mips arm RPi
	@echo "=== Built all x86 and cross-compiler targets ==="

clean:
	@echo "=== Cleaning intermediate build products ==="
	rm -rf ./*.o

distclean: clean
	@echo "=== Cleaning deployment products ==="
	rm -rf ./dist

dist:
	@echo "=== Creating deployment directory ==="
	mkdir -p dist

printver:
	@echo "=== Building $(DISTNAME) version $(PVERSION) ==="

x86: printver dist
	@echo "=== Building x86 ==="
	$(CC32) $(CFLAGS_D) $(LDFLAGS_D) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.debug.dynamic
	$(CC32) $(CFLAGS_P) $(LDFLAGS_P) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.performance.dynamic
#	$(CC32) $(CFLAGS_D) -static $(LDFLAGS_D) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.debug.static
#	$(CC32) $(CFLAGS_P) -static $(LDFLAGS_P) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.performance.static
	$(STRIP) dist/$(DISTNAME).$@.performance.*
	$(UPX) dist/$(DISTNAME).$@.performance.*
	rm -f dist/$(DISTNAME).$(PVERSION).$@.zip
	$(PCMD) dist/$(DISTNAME).$(PVERSION).$@.zip $(PFILES)

x86_64: printver dist
	@echo "=== Building x86-64 ==="
	$(CC64) $(CFLAGS_D) $(LDFLAGS_D) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.debug.dynamic
	$(CC64) $(CFLAGS_P) $(LDFLAGS_P) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.performance.dynamic
#	$(CC64) $(CFLAGS_D) -static $(LDFLAGS_D) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.debug.static
#	$(CC64) $(CFLAGS_P) -static $(LDFLAGS_P) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.performance.static
	$(STRIP) dist/$(DISTNAME).$@.performance.*
	$(UPX) dist/$(DISTNAME).$@.performance.*
	rm -f dist/$(DISTNAME).$(PVERSION).$@.zip
	$(PCMD) dist/$(DISTNAME).$(PVERSION).$@.zip $(PFILES)

mips: printver dist
	@echo "=== Building MIPS ==="
	$(MIPSPATH) $(MIPSCC) $(CFLAGS_D) $(LDFLAGS_D) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.debug.dynamic
	$(MIPSPATH) $(MIPSCC) $(CFLAGS_P) $(LDFLAGS_P) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.performance.dynamic
	$(MIPSPATH) $(MIPSCC) $(CFLAGS_D) -static $(LDFLAGS_D) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.debug.static
	$(MIPSPATH) $(MIPSCC) $(CFLAGS_P) -static $(LDFLAGS_P) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.performance.static
	$(MIPSPATH) $(MIPSSTRIP) dist/$(DISTNAME).$@.performance.*
	$(UPX) dist/$(DISTNAME).$@.performance.*
	rm -f dist/$(DISTNAME).$(PVERSION).$@.zip
	$(PCMD) dist/$(DISTNAME).$(PVERSION).$@.zip $(PFILES)

arm: printver dist
	@echo "=== Building ARM ==="
	$(ARMPATH) $(ARMCC) $(CFLAGS_D) $(LDFLAGS_D) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.debug.dynamic
	$(ARMPATH) $(ARMCC) $(CFLAGS_P) $(LDFLAGS_P) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.performance.dynamic
	$(ARMPATH) $(ARMCC) $(CFLAGS_D) -static $(LDFLAGS_D) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.debug.static
	$(ARMPATH) $(ARMCC) $(CFLAGS_P) -static $(LDFLAGS_P) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.performance.static
	$(ARMPATH) $(ARMSTRIP) dist/$(DISTNAME).$@.performance.*
	$(UPX) dist/$(DISTNAME).$@.performance.*
	rm -f dist/$(DISTNAME).$(PVERSION).$@.zip
	$(PCMD) dist/$(DISTNAME).$(PVERSION).$@.zip $(PFILES)

tomatoware: printver dist
	@echo "=== Building tomatoware ==="
	$(CC) $(CFLAGS_D) $(LDFLAGS_D) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.debug.dynamic
	$(CC) $(CFLAGS_P) $(LDFLAGS_P) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.performance.dynamic
	$(CC) $(CFLAGS_D) -static $(LDFLAGS_D) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.debug.static
	$(CC) $(CFLAGS_P) -static $(LDFLAGS_P) $(OPTS) $(SRCS) -o dist/$(DISTNAME).$@.performance.static
	$(STRIP) dist/$(DISTNAME).$@.performance.*
	$(UPX) dist/$(DISTNAME).$@.performance.*
	rm -f dist/$(DISTNAME).$(PVERSION).$@.zip
	$(PCMD) dist/$(DISTNAME).$(PVERSION).$@.zip $(PFILES)

RPi: printver dist
	@echo "=== Building for Raspberry Pi ==="
	$(RPIPATH) $(RPICC) $(CFLAGS_D) $(LDFLAGS_D) $(OPTS) $(RPIOPTS) $(SRCS) -o dist/$(DISTNAME).$@.debug.dynamic
	$(RPIPATH) $(RPICC) $(CFLAGS_P) $(LDFLAGS_P) $(OPTS) $(RPIOPTS) $(SRCS) -o dist/$(DISTNAME).$@.performance.dynamic
#	$(RPIPATH) $(RPICC) $(CFLAGS_D) -static $(LDFLAGS_D) $(OPTS) $(RPIOPTS) $(SRCS) -o dist/$(DISTNAME).$@.debug.static
#	$(RPIPATH) $(RPICC) $(CFLAGS_P) -static $(LDFLAGS_P) $(OPTS) $(RPIOPTS) $(SRCS) -o dist/$(DISTNAME).$@.performance.static
	$(RPIPATH) $(RPISTRIP) dist/$(DISTNAME).$@.performance.*
	$(UPX) dist/$(DISTNAME).$@.performance.*
	rm -f dist/$(DISTNAME).$(PVERSION).$@.zip
	$(PCMD) dist/$(DISTNAME).$(PVERSION).$@.zip $(PFILES)

