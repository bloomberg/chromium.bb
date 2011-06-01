// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FIND_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FIND_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/thunk/ppb_find_api.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

// Some of the backend functionality of this class is implemented by the
// AudioImpl so it can be shared with the proxy.
class PPB_Find_Impl : public ::ppapi::FunctionGroupBase,
                      public ::ppapi::thunk::PPB_Find_FunctionAPI {
 public:
  PPB_Find_Impl(PluginInstance* instance);
  virtual ~PPB_Find_Impl();

  // FunctionBase overrides.
  virtual ::ppapi::thunk::PPB_Find_FunctionAPI* AsPPB_Find_FunctionAPI()
      OVERRIDE;

  // PPB_Find_API implementation.
  virtual void NumberOfFindResultsChanged(PP_Instance instance,
                                          int32_t total,
                                          PP_Bool final_result) OVERRIDE;
  virtual void SelectedFindResultChanged(PP_Instance instance,
                                         int32_t index) OVERRIDE;

 private:
  PluginInstance* instance_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Find_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FIND_IMPL_H_
