// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_INSTANCE_IMPL_H_
#define PPAPI_SHARED_IMPL_INSTANCE_IMPL_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT InstanceImpl {
 public:
  virtual ~InstanceImpl();

  // Error checks the given resquest to Request[Filtering]InputEvents. Returns
  // PP_OK if the given classes are all valid, PP_ERROR_NOTSUPPORTED if not.
  int32_t ValidateRequestInputEvents(bool is_filtering, uint32_t event_classes);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_INSTANCE_IMPL_H_
