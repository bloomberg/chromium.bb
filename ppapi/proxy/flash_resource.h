// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FLASH_RESOURCE_H_
#define PPAPI_PROXY_FLASH_RESOURCE_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_flash_functions_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT FlashResource
    : public PluginResource,
      public NON_EXPORTED_BASE(thunk::PPB_Flash_Functions_API) {
 public:
  FlashResource(Connection connection, PP_Instance instance);
  virtual ~FlashResource();

  // Resource override.
  virtual thunk::PPB_Flash_Functions_API* AsPPB_Flash_Functions_API() OVERRIDE;

  // PPB_Flash_Functions_API implementation.
  virtual PP_Var GetProxyForURL(PP_Instance instance,
                                const std::string& url) OVERRIDE;
  virtual void UpdateActivity(PP_Instance instance) OVERRIDE;
  virtual PP_Bool SetCrashData(PP_Instance instance,
                               PP_FlashCrashKey key,
                               PP_Var value) OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(FlashResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FLASH_RESOURCE_H_
