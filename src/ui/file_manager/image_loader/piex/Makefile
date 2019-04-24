# Makefile for piex-wasm

PIEX = node_modules/piex

PSRC = $(PIEX)/src/piex.cc \
       $(PIEX)/src/tiff_parser.cc \
       $(PIEX)/src/image_type_recognition/image_type_recognition_lite.cc \
       $(PIEX)/src/tiff_directory/tiff_directory.cc \
       $(PIEX)/src/binary_parse/range_checked_byte_ptr.cc \
       $(PIEX)/src/binary_parse/cached_paged_byte_array.cc

WAPI = ["cwrap", "addFunction", "removeFunction"]

INCS = -I $(PIEX) -I .
WASM = -s WASM=1 -fno-exceptions -Wall
WOPT = -Os --llvm-opts 3 \
  -s STRICT=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s EXTRA_EXPORTED_RUNTIME_METHODS='$(WAPI)' \
  -s RESERVED_FUNCTION_POINTERS=2

all: a.out.js

a.out.js: piex.cpp.bc piex.src.bc
	em++ --bind --std=c++11 $(WASM) $(WOPT) $^

piex.cpp.bc: piex.cpp Makefile
	em++ --bind --std=c++11 $(WASM) $(WOPT) $(INCS) piex.cpp -o $@

piex.src.bc: $(PSRC) Makefile
	em++ --bind --std=c++11 $(WASM) $(WOPT) $(INCS) $(PSRC) -o $@

.PHONY: clean

clean:
	$(shell rm -f tests.hash tests.log package-lock* *.bc a.out.*)
