TARGET = tlc
INCLUDE = -I../include
LIBPATHS = 
FMODOBJ = tlc.o
CC = clang++
CFLAGS = -g -std=c++11 -Wall -Wno-deprecated-declarations -Wno-format-security
LFLAGS = -g
LIB_SRC = ../include/pv.h ../include/pv_bitwidth.h ../include/pv_macros.h ../include/pv_module.h \
 ../include/pv_register.h ../include/pv_testbench.h ../include/pv_value.h ../include/pv_vcd.h ../include/pv_wires.h

$(TARGET) : $(FMODOBJ)
	$(CC) $(LFLAGS) -o $@ $(FMODOBJ) $(LIBPATHS)

tlc.o : tlc.cc tlc.h $(LIB_SRC)

.cc.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(INCLUDE) $<

.PHONY: clean
clean:
	rm -f $(TARGET) *.o
