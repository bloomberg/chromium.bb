// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FONT_PROXY_H_
#define PPAPI_PROXY_PPB_FONT_PROXY_H_

#include "base/callback_forward.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_font_api.h"

namespace ppapi {
namespace proxy {

class PPB_Font_Proxy : public InterfaceProxy,
                       public ppapi::thunk::PPB_Font_FunctionAPI {
 public:
  explicit PPB_Font_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Font_Proxy();

  // FunctionGroupBase overrides.
  virtual ppapi::thunk::PPB_Font_FunctionAPI* AsPPB_Font_FunctionAPI() OVERRIDE;

  // PPB_Font_FunctionAPI implementation.
  virtual PP_Var GetFontFamilies(PP_Instance instance) OVERRIDE;

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  static const ApiID kApiID = API_ID_PPB_FONT;

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_Font_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FONT_PROXY_H_
