/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_INCLUDE_PORTABILITY_BITS_H_
#define NATIVE_CLIENT_SRC_INCLUDE_PORTABILITY_BITS_H_ 1

/*
  Portable builtin bit function overloads.

  The datatypes used in these overloads don't follow the coding convention:
  the goal is to dispatch user code to the appropriate intrinsic, and the
  intrinsics don't use uint*_t types. This means that this code could match
  the compiler- and target-specific mapping of stdint.h, or it could simply
  rely on overload resolution and let the compiler do this lookup (arguably
  less error-prone).

  The overall goal is to provide uint{8,16,32,64}_t, overloads to all the
  bit functions while dispatching to the most appropriate compiler intrinsic.

  See:
    - http://msdn.microsoft.com/en-us/library/bb385231.aspx
    - http://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html

  TODO(jfb) Add other bit functions like clz/ctz.
 */

#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/include/nacl_macros.h"

namespace nacl {

template<typename T> INLINE int PopCount(T /* v */) {
  // Only the specialized templates should be instantiated, getting
  // a compilation error here means an unsupported type was used.
  NACL_COMPILE_TIME_ASSERT(0);
  return 0;
}

#if NACL_WINDOWS
// See above file comment for explanation on datatype style guide violation.
template<> INLINE int PopCount<unsigned short>(unsigned short v) {
  return __popcnt16(v);
}
template<> INLINE int PopCount<unsigned int>(unsigned int v) {
  return __popcnt(v);
}
template<> INLINE int PopCount<unsigned __int64>(unsigned __int64 v) {
  return __popcnt64(v);
}
#else
// See above file comment for explanation on datatype style guide violation.
template<> INLINE int PopCount<unsigned short>(unsigned short v) {
  return __builtin_popcount(v);
}
template<> INLINE int PopCount<unsigned>(unsigned v) {
  return __builtin_popcount(v);
}
template<> INLINE int PopCount<unsigned long>(unsigned long v) {
  return __builtin_popcountl(v);
}
template<> INLINE int PopCount<unsigned long long>(unsigned long long v) {
  return __builtin_popcountll(v);
}
#endif

// There is no compiler-specific overload for this one, forward to uint16_t.
template<> INLINE int PopCount<uint8_t>(uint8_t v) {
  return nacl::PopCount<uint16_t>(v);
}

}  // namespace nacl

#endif  /* NATIVE_CLIENT_SRC_INCLUDE_PORTABILITY_BITS_H_ */
