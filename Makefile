#
# Makefile for BearGB
#

# Define this to build for SDL, otherwise build for circle
#USESDL = 1

EXTDIR = ext
SRCDIR = src
CIRCLESTDLIBHOME = $(EXTDIR)/circle-stdlib
CIRCLEHOME = $(EXTDIR)/circle-stdlib/libs/circle
NEWLIBDIR = $(EXTDIR)/circle-stdlib/install/$(NEWLIB_ARCH)

OBJS = \
	$(EXTDIR)/Blip_Buffer.o \
	$(SRCDIR)/GameBoy.o \
	$(SRCDIR)/GameBoyCart.o \
	$(SRCDIR)/GameBoyCpu.o \
	$(SRCDIR)/GameBoyPpu.o \
	$(SRCDIR)/GameBoyApu.o \
	$(SRCDIR)/GameBoySquareChannel.o \
	$(SRCDIR)/GameBoyNoiseChannel.o \
	$(SRCDIR)/GameBoyWaveChannel.o \
	$(SRCDIR)/OpenRomMenu.o

ifdef USESDL
	OBJS += $(SRCDIR)/SdlApp.o
	CPP = g++
	CPPFLAGS = -I$(SRCDIR) -I$(EXTDIR) -O3 -DUSE_SDL -std=c++17 `sdl2-config --cflags`
	NAME = beargb_sdl

$(NAME): $(OBJS)
	@echo "  LINK   $@"
	@$(CPP) -o $(NAME) $(OBJS) `sdl2-config --libs`

%.o: %.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS)

else # !USESDL
	OPTIMIZE = -Ofast -frename-registers

	OBJS += \
		$(SRCDIR)/CircleKernel.o \

	-include $(CIRCLESTDLIBHOME)/Config.mk
	include $(CIRCLEHOME)/Rules.mk

	CFLAGS += -I "$(NEWLIBDIR)/include" -I "$(STDDEF_INCPATH)" -I "$(CIRCLESTDLIBHOME)/include" -I$(EXTDIR) -DUSE_CIRCLE
	CPPFLAGS := $(subst c++14,c++17,$(CPPFLAGS))
	EXTRACLEAN += src/*.o ext/*.o bios/*.o

	LIBS := "$(NEWLIBDIR)/lib/libm.a" "$(NEWLIBDIR)/lib/libc.a" "$(NEWLIBDIR)/lib/libcirclenewlib.a" \
		$(CIRCLEHOME)/addon/SDCard/libsdcard.a \
		$(CIRCLEHOME)/lib/usb/libusb.a \
		$(CIRCLEHOME)/lib/input/libinput.a \
		$(CIRCLEHOME)/addon/fatfs/libfatfs.a \
		$(CIRCLEHOME)/lib/fs/libfs.a \
		$(CIRCLEHOME)/lib/net/libnet.a \
		$(CIRCLEHOME)/lib/sched/libsched.a \
		$(CIRCLEHOME)/lib/libcircle.a

endif

circle:
	cd $(CIRCLESTDLIBHOME) && ./configure && make