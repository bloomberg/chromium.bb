// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_RESOURCE_ARRAY_API_H_
#define PPAPI_THUNK_PPB_RESOURCE_ARRAY_API_H_

#include "ppapi/c/dev/ppb_resource_array_dev.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_ResourceArray_API {
 public:
  virtual ~PPB_ResourceArray_API() {}

  virtual uint32_t GetSize() = 0;
  virtual PP_Resource GetAt(uint32_t index) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_RESOURCE_ARRAY_API_H_
