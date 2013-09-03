HOME = ../..

include $(HOME)/Defines.mk

LIBS += -L$(HOME)/$(LIB_DIR) -l$(PHOENIX)

PROGS := mysort mysort-seq mysort-openmp mysort-openmp2
MYSORT_OBJS := mysort.o 
MYSORT_SEQ_OBJS := mysort-seq.o 
MYSORT_OPENMP_OBJS := mysort-openmp.o
MYSORT_OPENMP2_OBJS := mysort-openmp2.o

.PHONY: default all clean

default: all

all: $(PROGS)

mysort: $(MYSORT_OBJS) $(LIB_DEP)
	$(CXX) $(CFLAGS) -o $@ $(MYSORT_OBJS) $(LIBS)

mysort-seq: $(MYSORT_SEQ_OBJS) $(LIB_DEP)
	$(CXX) $(CFLAGS) -o $@ $(MYSORT_SEQ_OBJS) $(LIBS)

mysort-openmp: $(MYSORT_OPENMP_OBJS) $(LIB_DEP)
	$(CXX) $(CFLAGS) -O3 -fopenmp -o $@ $(MYSORT_OPENMP_OBJS) $(LIBS)
mysort-openmp2: $(MYSORT_OPENMP2_OBJS) $(LIB_DEP)
	$(CXX) $(CFLAGS) -O3 -fopenmp -o $@ $(MYSORT_OPENMP2_OBJS) $(LIBS)

#%.o: %.cpp
#	$(CXX) $(CFLAGS) -c $< -o $@ -I$(HOME)/$(INC_DIR)

mysort.o: mysort.cpp
	$(CXX) $(CFLAGS) -c $< -o $@ -I$(HOME)/$(INC_DIR)

mysort-seq.o: mysort-seq.cpp
	$(CXX) $(CFLAGS) -std=c++0x -c $< -o $@ -I$(HOME)/$(INC_DIR)

mysort-openmp.o: mysort-openmp.cpp
	$(CXX) $(CFLAGS) -std=c++0x -O3 -fopenmp -c $< -o $@ -I$(HOME)/$(INC_DIR)
mysort-openmp2.o: mysort-openmp2.cpp
	$(CXX) $(CFLAGS) -std=c++0x -O3 -fopenmp -c $< -o $@ -I$(HOME)/$(INC_DIR)

clean:
	rm -f $(PROGS) $(MYSORT_OBJS) $(MYSORT_SEQ_OBJS) $(MYSORT_OPENMP_OBJS) $(MYSORT_OPENMP2_OBJS)
