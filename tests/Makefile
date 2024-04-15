include ../config.mk

CXXFLAGS+=-I../${DOCTEST_HEADERS}

.PHONY: all clean check

all: check

check: tests
	./tests

SRC = $(wildcard test_*.cpp)
OBJ = $(subst .cpp,.o, $(SRC))

tests: tests.o $(OBJ) 

SEPARATOR=+
NAMES = $(subst test_,,$(basename $(SRC)))
TEMPLATE = $(join test_, $(1)).o: $(join test_, $(1)).cpp $(foreach h, $(subst $(SEPARATOR), ,$(1)), $(join ../include/calculisto/${PROJECT}/, $(h)).hpp)
$(foreach name, $(NAMES), $(eval $(call TEMPLATE,$(name))))

clean: 
	rm -f tests *.o 
