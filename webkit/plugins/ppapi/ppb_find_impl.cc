// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_find_impl.h"

#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using ::ppapi::thunk::PPB_Find_FunctionAPI;

namespace webkit {
namespace ppapi {

PPB_Find_Impl::PPB_Find_Impl(PluginInstance* instance) : instance_(instance) {
}

PPB_Find_Impl::~PPB_Find_Impl() {
}

PPB_Find_FunctionAPI* PPB_Find_Impl::AsPPB_Find_FunctionAPI() {
  return this;
}

void PPB_Find_Impl::NumberOfFindResultsChanged(PP_Instance instance,
                                               int32_t total,
                                               PP_Bool final_result) {
  DCHECK_NE(instance_->find_identifier(), -1);
  instance_->delegate()->NumberOfFindResultsChanged(
      instance_->find_identifier(), total, PP_ToBool(final_result));
}

void PPB_Find_Impl::SelectedFindResultChanged(PP_Instance instance,
                                              int32_t index) {
  DCHECK_NE(instance_->find_identifier(), -1);
  instance_->delegate()->SelectedFindResultChanged(
      instance_->find_identifier(), index);
}

}  // namespace ppapi
}  // namespace webkit
