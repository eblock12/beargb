#
# Makefile for BearGB
#

CIRCLEHOME = ext/circle
SRCDIR = src

CPPFLAGS = -I$(SRCDIR) -Wall -Werror -O3

OBJS = \
	$(SRCDIR)/main.o \
	$(SRCDIR)/kernel.o

LIBS = \
	$(CIRCLEHOME)/lib/usb/libusb.a \
	$(CIRCLEHOME)/lib/input/libinput.a \
	$(CIRCLEHOME)/lib/fs/libfs.a \
	$(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

circle:
	cd ext/circle && ./makeall clean && ./makeall