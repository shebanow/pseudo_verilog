TARGET = xxx
# INCLUDE = -I../../fixed_point/include -I../../pseudo_verilog
LIBPATHS = 
# FMODOBJ = main.o simulator.o iAcoustic.o adc_sh.o framing.o windowing.o fft.o nn.o sio.o rcpt.o csv.o
FMODOBJ = xxx.o
CC = clang++
CFLAGS = -g -std=c++11 -Wall -Wno-deprecated-declarations -Wno-format-security
LFLAGS = -g

$(TARGET) : $(FMODOBJ)
	$(CC) $(LFLAGS) -o $@ $(FMODOBJ) $(LIBPATHS)

xxx.o : xxx.cc pv_global.h
# main.o : main.h simulator.h tlc.h main.cc
# simulator.o: main.h simulator.h simulator.cc
# iAcoustic.o: main.h simulator.h iAcoustic.h iAcoustic.cc
# adc_sh.o : ../../fixed_point/include/fixed_point.h main.h simulator.h adc_sh.h adc_sh.cc
# framing.o : main.h simulator.h spram.h framing.h framing.cc
# windowing.o : main.h simulator.h spram.h windowing.h windowing.cc
# fft.o : main.h simulator.h spram.h fft.h fft.cc
# nn.o : main.h spram.h simulator.h nn.h nn.cc
# sio.o : main.h simulator.h sio.h sio.cc
# rcpt.o : main.h simulator.h rcpt.h rcpt.cc
# csv.o : csv.h csv.cc

.cc.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(INCLUDE) $<

.PHONY: clean
clean:
	rm -f $(TARGET) *.o
