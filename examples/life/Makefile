RM := /bin/rm -f

CXX		:= g++
CFLAGS  := -g -Wall -O3 -std=gnu++0x -I../../include

LIBHPPFILES := $(wildcard ../../include/*/*.hpp) 

CPPFILES := $(wildcard *.cpp)
HPPFILES := $(wildcard *.hpp)
OBJFILES := $(CPPFILES:%.cpp=objs/%.o)

LIBDESCORE := ../../objs/descore/libdescore.a
LIBCASCADE := ../../objs/cascade/libcascade.a

life: $(LIBDESCORE) $(LIBCASCADE) $(OBJFILES) 
	$(CXX) $(OBJFILES) $(LIBCASCADE) $(LIBDESCORE) -lpthread -lz -ltermcap -o $@

objs/%.o: %.cpp $(HPPFILES) $(LIBHPPFILES) 
	$(CXX) $(CFLAGS) $(ARGS) -c -o $@ $<

$(LIBDESCORE):
	cd ../../src/descore; make

$(LIBCASCADE):
	cd ../../src/cascade; make

clean:
	$(RM) $(OBJFILES)
	$(RM) life
