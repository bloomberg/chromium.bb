/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_BASE_UTILS_H_
#define INCLUDE_PERFETTO_BASE_UTILS_H_

#include "perfetto/base/build_config.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

#define PERFETTO_EINTR(x)                                   \
  ({                                                        \
    decltype(x) eintr_wrapper_result;                       \
    do {                                                    \
      eintr_wrapper_result = (x);                           \
    } while (eintr_wrapper_result == -1 && errno == EINTR); \
    eintr_wrapper_result;                                   \
  })

#define PERFETTO_LIKELY(_x) __builtin_expect(!!(_x), 1)
#define PERFETTO_UNLIKELY(_x) __builtin_expect(!!(_x), 0)

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
// TODO(brucedawson) - create a ::perfetto::base::IOSize to replace this.
#if defined(_WIN64)
using ssize_t = __int64;
#else
using ssize_t = long;
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PERFETTO_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define PERFETTO_WARN_UNUSED_RESULT
#endif

#if defined(__clang__)
#define PERFETTO_ALWAYS_INLINE __attribute__((__always_inline__))
#else
// GCC is too pedantic and often fails with the error:
// "always_inline function might not be inlinable"
#define PERFETTO_ALWAYS_INLINE
#endif

namespace perfetto {
namespace base {

constexpr size_t kPageSize = 4096;
constexpr size_t kMaxCpus = 128;

template <typename T>
constexpr size_t ArraySize(const T& array) {
  return sizeof(array) / sizeof(array[0]);
}

template <typename... T>
inline void ignore_result(const T&...) {}

// Function object which invokes 'free' on its parameter, which must be
// a pointer. Can be used to store malloc-allocated pointers in std::unique_ptr:
//
// std::unique_ptr<int, base::FreeDeleter> foo_ptr(
//     static_cast<int*>(malloc(sizeof(int))));
struct FreeDeleter {
  inline void operator()(void* ptr) const { free(ptr); }
};

template <typename T>
constexpr T AssumeLittleEndian(T value) {
  static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
                "Unimplemented on big-endian archs");
  return value;
}

// Round up |size| to a multiple of |alignment| (must be a power of two).
template <size_t alignment>
constexpr size_t AlignUp(size_t size) {
  static_assert((alignment & (alignment - 1)) == 0, "alignment must be a pow2");
  return (size + alignment - 1) & ~(alignment - 1);
}

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_BASE_UTILS_H_
