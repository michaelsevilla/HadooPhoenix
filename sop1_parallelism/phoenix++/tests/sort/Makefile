HOME = ../..

include $(HOME)/Defines.mk

LIBS += -L$(HOME)/$(LIB_DIR) -l$(PHOENIX)

PROGS := mysort mysort-seq mysort-ingest
MYSORT_OBJS := mysort.o 
MYSORT_SEQ_OBJS := mysort-seq.o 
MYSORT_INGEST_OBJS := mysort-ingest.o 

.PHONY: default all clean

default: all

all: $(PROGS)

mysort: $(MYSORT_OBJS) $(LIB_DEP)
	$(CXX) $(CFLAGS) -o $@ $(MYSORT_OBJS) $(LIBS)

mysort-seq: $(MYSORT_SEQ_OBJS) $(LIB_DEP)
	$(CXX) $(CFLAGS) -o $@ $(MYSORT_SEQ_OBJS) $(LIBS)

mysort-ingest: $(MYSORT_INGEST_OBJS) $(LIB_DEP)
	$(CXX) $(CFLAGS) -o $@ $(MYSORT_INGEST_OBJS) $(LIBS)

#mysort-openmp: $(MYSORT_OPENMP_OBJS) $(LIB_DEP)
#	$(CXX) $(CFLAGS) -O3 -fopenmp -o $@ $(MYSORT_OPENMP_OBJS) $(LIBS)

#%.o: %.cpp
#	$(CXX) $(CFLAGS) -c $< -o $@ -I$(HOME)/$(INC_DIR)

mysort.o: mysort.cpp
	$(CXX) $(CFLAGS) -c $< -o $@ -I$(HOME)/$(INC_DIR)

mysort-seq.o: mysort-seq.cpp
	$(CXX) $(CFLAGS) -std=c++0x -c $< -o $@ -I$(HOME)/$(INC_DIR)

mysort-ingest.o: mysort-ingest.cpp
	$(CXX) $(CFLAGS) -std=c++0x -c $< -o $@ -I$(HOME)/$(INC_DIR)

#mysort-openmp.o: mysort-openmp.cpp
#	$(CXX) $(CFLAGS) -std=c++0x -O3 -fopenmp -c $< -o $@ -I$(HOME)/$(INC_DIR)

clean:
	rm -f $(PROGS) $(MYSORT_OBJS) $(MYSORT_SEQ_OBJS) $(MYSORT_INGEST_OBJS)
