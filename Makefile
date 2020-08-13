#
# Makefile for BearGB
#

# Define this to build for SDL, otherwise build for circle
#USESDL = 1

SRCDIR = src
CIRCLEHOME = ext/circle

OBJS = \
	$(SRCDIR)/main.o \
	$(SRCDIR)/kernel.o

ifdef USESDL
	CPP = g++
	CPPFLAGS = -I$(SRCDIR) -O3 -DUSE_SDL `sdl-config --cflags`
	NAME = beargb_sdl

$(NAME): $(OBJS)
	@echo "  LINK   $@"
	@$(CPP) -o $(NAME) $(OBJS) `sdl-config --libs`

%.o: %.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS)

else # !USESDL

	CPPFLAGS = -I$(SRCDIR) -Wall -Werror -O3 -DUSE_CIRCLE

	LIBS = \
		$(CIRCLEHOME)/lib/usb/libusb.a \
		$(CIRCLEHOME)/lib/input/libinput.a \
		$(CIRCLEHOME)/lib/fs/libfs.a \
		$(CIRCLEHOME)/lib/libcircle.a

	include $(CIRCLEHOME)/Rules.mk

endif

circle:
	$(CIRCLEHOME) && ./makeall clean && ./makeall
