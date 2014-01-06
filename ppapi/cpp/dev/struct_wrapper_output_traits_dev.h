// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_STRUCT_WRAPPER_OUTPUT_TRAITS_DEV_
#define PPAPI_CPP_DEV_STRUCT_WRAPPER_OUTPUT_TRAITS_DEV_

namespace pp {
namespace internal {

// This class is used as the base class for CallbackOutputTraits specialized for
// C struct wrappers.
template <typename T>
struct StructWrapperOutputTraits {
  typedef typename T::CType* APIArgType;
  typedef T StorageType;

  // Returns the underlying C struct of |t|, which can be passed to the browser
  // as an output parameter.
  static inline APIArgType StorageToAPIArg(StorageType& t) {
    return t.StartRawUpdate();
  }

  // For each |t| passed into StorageToAPIArg(), this method must be called
  // exactly once in order to match StartRawUpdate() and EndRawUpdate().
  static inline T& StorageToPluginArg(StorageType& t) {
    t.EndRawUpdate();
    return t;
  }

  static inline void Initialize(StorageType* /* t */) {}
};

}  // namespace internal
}  // namespace pp

#endif  // PPAPI_CPP_DEV_STRUCT_WRAPPER_OUTPUT_TRAITS_DEV_
