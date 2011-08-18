// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FIND_API_H_
#define PPAPI_THUNK_PPB_FIND_API_H_

#include "ppapi/c/dev/ppb_find_dev.h"
#include "ppapi/proxy/interface_id.h"

namespace ppapi {
namespace thunk {

class PPB_Find_FunctionAPI {
 public:
  virtual ~PPB_Find_FunctionAPI() {}

  virtual void NumberOfFindResultsChanged(PP_Instance instance,
                                          int32_t total,
                                          PP_Bool final_result) = 0;
  virtual void SelectedFindResultChanged(PP_Instance instance,
                                         int32_t index) = 0;

  static const proxy::InterfaceID interface_id = proxy::INTERFACE_ID_PPB_FIND;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_FIND_API_H_
