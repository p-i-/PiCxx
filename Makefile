PYTHON_CONFIG?=python3-config
override CXXFLAGS+=`$(PYTHON_CONFIG) --includes` -IPiCxx/headers -std=c++11
override LDFLAGS+=`$(PYTHON_CONFIG) --ldflags` -Lbuild -lpicxx
USRDIR?=/usr/local
HDRDIR?=$(USRDIR)/include
LIBDIR?=$(USRDIR)/lib
INSTALL?=install

.PHONY : all test install

$(shell mkdir -p build/py)

objects := $(patsubst PiCxx/Src/%.cxx,build/%.o,$(wildcard PiCxx/Src/*.cxx))

all : build/libpicxx.a

test : build/test build/py/test_funcmapper.py
	cd build && ./test

build/py/test_funcmapper.py : test_PiCxx/test_funcmapper.py
	cp $< $@

build/test : test_PiCxx/main.cpp test_PiCxx/*.cxx test_PiCxx/*.hxx build/libpicxx.a
	$(CXX) $(CXXFLAGS) -o $@ test_PiCxx/*.cpp test_PiCxx/*.cxx $(LDFLAGS)

build/libpicxx.a : $(objects)
	ar rcs $@ $<

$(objects) : build/%.o : PiCxx/Src/%.cxx PiCxx/headers/*.hxx PiCxx/headers/Base/*.h* PiCxx/headers/ExtObj/*.hxx
	$(CXX) $(CXXFLAGS) -c -o $@ $<

install : build/libpicxx.a
	mkdir -p $(HDRDIR)/PiCxx
	find PiCxx/headers -type f -exec echo Installing {} \; -exec $(INSTALL) {} $(HDRDIR)/PiCxx \;
	$(INSTALL) $< $(LIBDIR)
