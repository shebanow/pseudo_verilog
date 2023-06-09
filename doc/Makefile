LIB_SRC = ../include/pv.h ../include/pv_bitwidth.h ../include/pv_macros.h ../include/pv_module.h \
 ../include/pv_register.h ../include/pv_testbench.h ../include/pv_value.h ../include/pv_vcd.h ../include/pv_wires.h

doc: README.pdf PV.pdf

README.pdf : ../README.md
	pandoc ../README.md -V geometry:margin=1in -s -o README.pdf

PV.pdf : PV.md
	pandoc PV.md -V geometry:margin=1in -s -o PV.pdf

.PHONY: clean
clean:
	rm -f *.pdf
