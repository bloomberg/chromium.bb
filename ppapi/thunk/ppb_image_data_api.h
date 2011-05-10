// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_IMAGE_DATA_API_H_
#define PPAPI_THUNK_PPB_IMAGE_DATA_API_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/ppb_image_data.h"

namespace ppapi {
namespace thunk {

class PPB_ImageData_API {
 public:
  virtual PP_Bool Describe(PP_ImageDataDesc* desc) = 0;
  virtual void* Map() = 0;
  virtual void Unmap() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_IMAGE_DATA_API_H_
