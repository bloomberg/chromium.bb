// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FLASH_PROXY_H_
#define PPAPI_PROXY_PPB_FLASH_PROXY_H_

#include <string>

#include "base/compiler_specific.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/thunk/ppb_flash_api.h"

struct PPB_Flash_Print_1_0;

namespace ppapi {

struct URLRequestInfoData;

namespace proxy {

struct PPBFlash_DrawGlyphs_Params;
struct SerializedDirEntry;
class SerializedVarReturnValue;

/////////////////////////// WARNING:DEPRECTATED ////////////////////////////////
// Please do not add any new functions to this proxy. They should be
// implemented in the new-style resource proxy (see flash_resource.h).
// TODO(raymes): All of these functions should be moved to the new-style proxy.
////////////////////////////////////////////////////////////////////////////////
class PPB_Flash_Proxy : public InterfaceProxy, public thunk::PPB_Flash_API {
 public:
  explicit PPB_Flash_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Flash_Proxy();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_FLASH;

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FLASH_PROXY_H_
