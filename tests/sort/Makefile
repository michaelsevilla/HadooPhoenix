HOME = ../..

include $(HOME)/Defines.mk

LIBS += -L$(HOME)/$(LIB_DIR) -l$(PHOENIX)

MYSORT_OBJS := mysort.o
MYSORT_SEQ_OBJS := mysort-seq.o
TERASORT_OBJS := terasort.o

PROGS := mysort mysort-seq terasort

.PHONY: default all clean

default: all

all: $(PROGS)

mysort: $(MYSORT_OBJS) $(LIB_DEP)
	$(CXX) $(CFLAGS) -o $@ $(MYSORT_OBJS) $(LIBS)

mysort-seq: $(MYSORT_SEQ_OBJS) $(LIB_DEP)
	$(CXX) $(CFLAGS) -o $@ $(MYSORT_SEQ_OBJS) $(LIBS)

terasort: $(TERASORT_OBJS) $(LIB_DEP)
	$(CXX) $(CFLAGS) -o $@ $(TERASORT_OBJS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@ -I$(HOME)/$(INC_DIR)

clean:
	rm -f $(PROGS) $(MYSORT_OBJS)
