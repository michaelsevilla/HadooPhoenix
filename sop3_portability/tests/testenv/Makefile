#------------------------------------------------------------------------------
# Written by msevilla (May 21, 2013
#------------------------------------------------------------------------------ 

# This Makefile requires GNU make.

HOME = ../..

include $(HOME)/Defines.mk

LIBS += -L$(HOME)/$(LIB_DIR) -l$(PHOENIX)

TESTENV_OBJS = testenv.o

PROGS = testenv 

.PHONY: default all clean

default: all

all: $(PROGS)

testenv: $(TESTENV_OBJS) $(LIB_DEP)
	$(CC) $(CFLAGS) -o $@ $(TESTENV_OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -I$(HOME)/$(INC_DIR)

clean:
	rm -f $(PROGS) $(TESTENV_OBJS)
