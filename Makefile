# NOTE: Feel free to change the makefile to suit your own need.

# make rules
TARGETS = dsdv

all: $(TARGETS)

dsdv: dsdv.cc
	g++ -Wall -g $^ -o $@ -pthread

clean:
	rm -f *~ *.o $(TARGETS)
