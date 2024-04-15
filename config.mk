DOCTEST_HEADERS=../../external/onqtam/doctest
DEPENDENCIES_HEADERS=                   \
	../../external/calculisto/uri/include     \
	../../external/taocpp/pegtl/include \
	../../external/taocpp/json/include

PROJECT=json_validator
LINK.o=${LINK.cc}
CXXFLAGS+=-std=c++2a -Wall -Wextra -I../${FMTLIB_HEADERS} $(foreach dir, ${DEPENDENCIES_HEADERS}, -I../${dir})
LDFLAGS+=
LDLIBS+= -lfmt

