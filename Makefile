#
# Makefile for BearGB
#

# Define this to build for SDL, otherwise build for circle
#USESDL = 1

SRCDIR = src
CIRCLESTDLIBHOME = ext/circle-stdlib
CIRCLEHOME = ext/circle-stdlib/libs/circle
NEWLIBDIR = ext/circle-stdlib/install/$(NEWLIB_ARCH)

OBJS = \
	$(SRCDIR)/main.o \
	$(SRCDIR)/Kernel.o \
	$(SRCDIR)/GameBoy.o \
	$(SRCDIR)/GameBoyCart.o \
	$(SRCDIR)/GameBoyCpu.o \
	$(SRCDIR)/GameBoyPpu.o

ifdef USESDL
	CPP = g++
	CPPFLAGS = -I$(SRCDIR) -O3 -DUSE_SDL -std=c++17 `sdl2-config --cflags`
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

	-include $(CIRCLESTDLIBHOME)/Config.mk
	include $(CIRCLEHOME)/Rules.mk

	CFLAGS += -I "$(NEWLIBDIR)/include" -I $(STDDEF_INCPATH) -std=c++17 -O3 -DUSE_CIRCLE

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