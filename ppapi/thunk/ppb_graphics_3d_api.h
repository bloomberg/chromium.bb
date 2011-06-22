// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_GRAPHICS_3D_API_H_
#define PPAPI_THUNK_PPB_GRAPHICS_3D_API_H_

#include "ppapi/c/dev/ppb_graphics_3d_dev.h"

namespace ppapi {
namespace thunk {

class PPB_Graphics3D_API {
 public:
  virtual int32_t GetAttribs(int32_t* attrib_list) = 0;
  virtual int32_t SetAttribs(int32_t* attrib_list) = 0;
  virtual int32_t SwapBuffers(PP_CompletionCallback callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_GRAPHICS_3D_API_H_
