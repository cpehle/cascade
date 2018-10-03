RM := /bin/rm -f

AR      := ar
ARFLAGS := rcs

CXX		:= g++
CFLAGS  := -g -O3 -Wall -std=gnu++0x -I../../include -I../../include/cascade

LIBHPPFILES := $(wildcard ../../include/descore/*.hpp)

CPPFILES  := $(wildcard *.cpp)
HPPFILES  := $(wildcard *.hpp)
OBJFILES  := $(CPPFILES:%.cpp=../../objs/cascade/%.o)
VOBJFILES := $(CPPFILES:%.cpp=../../objs/cascade_verilog/%.o)

../../objs/cascade/libcascade.a: $(OBJFILES)
	$(AR) $(ARFLAGS) $@ $(OBJFILES)

../../objs/cascade/%.o: %.cpp $(HPPFILES) $(LIBHPPFILES)
	$(CXX) $(CFLAGS) -c -o $@ $<

verilog: $(VOBJFILES)
	$(AR) $(ARFLAGS) ../../objs/cascade_verilog/libcascade_verilog.a $(VOBJFILES)

../../objs/cascade_verilog/%.o: %.cpp $(HPPFILES) $(LIBHPPFILES)
	$(CXX) $(CFLAGS) -D_VERILOG -I$(VCS_HOME)/include -c -o $@ $<

clean:
	$(RM) $(OBJFILES)
	$(RM) $(VOBJFILES)
	$(RM) ../../objs/cascade/libcascade.a
	$(RM) ../../objs/cascade_verilog/libcascade_verilog.a
