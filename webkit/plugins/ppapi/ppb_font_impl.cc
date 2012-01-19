// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_font_impl.h"

#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit {

namespace ppapi {

PPB_Font_FunctionImpl::PPB_Font_FunctionImpl(PluginInstance* instance)
    : instance_(instance) {
}

PPB_Font_FunctionImpl::~PPB_Font_FunctionImpl() {
}

::ppapi::thunk::PPB_Font_FunctionAPI*
PPB_Font_FunctionImpl::AsFont_FunctionAPI() {
  return this;
}

// TODO(ananta)
// We need to wire this up to the proxy.
PP_Var PPB_Font_FunctionImpl::GetFontFamilies(PP_Instance instance) {
  return PP_MakeUndefined();
}

}  // namespace ppapi

}  // namespace webkit
