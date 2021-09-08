/* Copyright 2019 Google LLC. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// Miscellaneous helpers internal library.

#ifndef RUY_RUY_COMMON_H_
#define RUY_RUY_COMMON_H_

#include <limits>
#include <type_traits>

#include "ruy/opt_set.h"

#if RUY_OPT(PREFETCH_LOAD)
#define RUY_PREFETCH_LOAD(X) X
#else
#define RUY_PREFETCH_LOAD(X)
#endif

#if RUY_OPT(PREFETCH_STORE)
#define RUY_PREFETCH_STORE(X) X
#else
#define RUY_PREFETCH_STORE(X)
#endif

#define RUY_STR(s) RUY_STR_UNEXPANDED(s)
#define RUY_STR_UNEXPANDED(s) #s

namespace ruy {

// Helper for type-erasing a pointer.
//
// Often inside Ruy, a template parameter holds type information statically, but
// we would like to have a function signature that doesn't depend on the
// template parameters, so that we can dispatch indirectly across multiple
// implementations. This helper is at the core of such type-erasure.
//
// The opposite of this operation is just `static_cast<T*>(void_ptr)`.
template <typename T>
void* ToVoidPtr(T* p) {
  return const_cast<void*>(static_cast<const void*>(p));
}

template <typename Scalar>
Scalar SymmetricZeroPoint() {
  if (std::is_floating_point<Scalar>::value) {
    return 0;
  }
  if (std::is_signed<Scalar>::value) {
    return 0;
  }
  return std::numeric_limits<Scalar>::max() / 2 + 1;
}

}  // namespace ruy

#endif  // RUY_RUY_COMMON_H_
