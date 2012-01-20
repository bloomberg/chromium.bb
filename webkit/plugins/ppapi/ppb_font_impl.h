// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FONT_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FONT_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/thunk/ppb_font_api.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_Font_FunctionImpl : public ::ppapi::FunctionGroupBase,
                              public ::ppapi::thunk::PPB_Font_FunctionAPI {
 public:
  explicit PPB_Font_FunctionImpl(PluginInstance* instance);
  virtual ~PPB_Font_FunctionImpl();

  // FunctionGroupBase overrides.
  virtual ::ppapi::thunk::PPB_Font_FunctionAPI* AsFont_FunctionAPI();

  // PPB_Font_FunctionAPI implementation.
  virtual PP_Var GetFontFamilies(PP_Instance instance) OVERRIDE;

 private:
  PluginInstance* instance_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Font_FunctionImpl);
};

}  // namespace ppapi

} // namespace webkit.

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FONT_IMPL_H_
