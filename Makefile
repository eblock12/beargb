#
# Makefile for BearGB
#

CIRCLEHOME = ext/circle
SRCDIR = src

CPPFLAGS = -I$(SRCDIR) -Wall -Werror -O3

OBJS = \
	$(SRCDIR)/main.o

include $(CIRCLEHOME)/sample/Rules.mk