// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_FUNCTION_GROUP_BASE_H_
#define PPAPI_SHARED_IMPL_FUNCTION_GROUP_BASE_H_

namespace ppapi {

namespace thunk {
class PPB_Font_FunctionAPI;
class ResourceCreationAPI;
}

class FunctionGroupBase {
 public:
  // Dynamic casting for this object. Returns the pointer to the given type if
  // it's supported.
  virtual thunk::PPB_Font_FunctionAPI* AsFont_FunctionAPI() { return NULL; }
  virtual thunk::ResourceCreationAPI* AsResourceCreation() { return NULL; }

  template <typename T> T* GetAs() { return NULL; }
};

template<>
inline thunk::PPB_Font_FunctionAPI* FunctionGroupBase::GetAs() {
  return AsFont_FunctionAPI();
}
template<>
inline ppapi::thunk::ResourceCreationAPI* FunctionGroupBase::GetAs() {
  return AsResourceCreation();
}

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_FUNCTION_GROUP_BASE_H_
