// This file is included by third_party/glibc/elf/elf.h
// We use it to map a couple of glibc specific macros
// over to their OS X equivalents.


// Map GNU endianness macros over to OS X endianness macros.

#ifndef __LITTLE_ENDIAN
  #define __LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#endif
#ifndef __BIG_ENDIAN
  #define __BIG_ENDIAN __ORDER_BIG_ENDIAN__
#endif
#ifndef __BYTE_ORDER
  #define __BYTE_ORDER __BYTE_ORDER__
#endif
