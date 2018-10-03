RM := /bin/rm -f

AR       := ar
ARFLAGS  := rcs

CXX	     := g++
CXXFLAGS := -g -Wall -O3 -std=gnu++0x -I../../include/descore

CPPFILES := $(wildcard *.cpp)
HPPFILES := $(wildcard *.hpp)
OBJFILES := $(CPPFILES:%.cpp=../../objs/descore/%.o)

../../objs/descore/libdescore.a: $(OBJFILES)
	$(AR) $(ARFLAGS) $@ $(OBJFILES)

../../objs/descore/%.o: %.cpp $(HPPFILES)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(OBJFILES)
	$(RM) ../../objs/descore/libdescore.a
