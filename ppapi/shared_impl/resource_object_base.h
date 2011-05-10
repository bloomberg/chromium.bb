// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_RESOURCE_OBJECT_BASE_H_
#define PPAPI_SHARED_IMPL_RESOURCE_OBJECT_BASE_H_

namespace ppapi {

namespace thunk {
class PPB_Font_API;
class PPB_Graphics2D_API;
class PPB_ImageData_API;
}

namespace shared_impl {

class ResourceObjectBase {
 public:

  virtual thunk::PPB_Font_API* AsFont_API() { return NULL; }
  virtual thunk::PPB_Graphics2D_API* AsGraphics2D_API() { return NULL; }
  virtual thunk::PPB_ImageData_API* AsImageData_API() { return NULL; }

  template <typename T> T* GetAs() { return NULL; }
};

template<>
inline thunk::PPB_Font_API* ResourceObjectBase::GetAs() {
  return AsFont_API();
}
template<>
inline thunk::PPB_Graphics2D_API* ResourceObjectBase::GetAs() {
  return AsGraphics2D_API();
}
template<>
inline thunk::PPB_ImageData_API* ResourceObjectBase::GetAs() {
  return AsImageData_API();
}

}  // namespace shared_impl
}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_RESOURCE_OBJECT_BASE_H_
