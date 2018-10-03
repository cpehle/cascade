#
# To use this example, the cascade library must be built with
#
#   env VERILOG=1 make
#
RM := /bin/rm -rf

VCS     := $(VCS_HOME)/bin/vcs
CXX		:= g++
CFLAGS  := -ggdb3 -Wall -std=gnu++0x -I../../include
VFLAGS  := -sverilog +vpi -P ../../verilog/vpi.tab -full64

LIBHPPFILES := $(wildcard ../../include/*/*.hpp) 

LIBDESCORE := ../../objs/descore/libdescore.a
LIBCASCADE := ../../objs/cascade_verilog/libcascade_verilog.a

simv: $(LIBDESCORE) $(LIBCASCADE) Adder.o tb_adder.sv CAdder.sv 
	$(VCS) $(VFLAGS) Adder.o CAdder.sv tb_adder.sv $(LIBCASCADE) $(LIBDESCORE) -lz -ltermcap 

Adder.o: Adder.cpp $(LIBHPPFILES)
	$(CXX) $(CFLAGS) -D_VERILOG -I$(VCS_HOME)/include -c -o Adder.o Adder.cpp

$(LIBDESCORE):
	cd ../../src/descore; make

$(LIBCASCADE):
	cd ../../src/cascade; make verilog

clean:
	$(RM) simv
	$(RM) Adder.o
	$(RM) csrc
	$(RM) simv.daidir
	$(RM) ucli.key
