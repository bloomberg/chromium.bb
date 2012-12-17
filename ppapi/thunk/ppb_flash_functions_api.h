// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FLASH_FUNCTIONS_API_H_
#define PPAPI_THUNK_PPB_FLASH_FUNCTIONS_API_H_

#include <string>

#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/shared_impl/singleton_resource_id.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

// This class collects all of the Flash interface-related APIs into one place.
// PPB_Flash_API is deprecated in favor of this (the new resource model uses
// this API).
class PPAPI_THUNK_EXPORT PPB_Flash_Functions_API {
 public:
  virtual ~PPB_Flash_Functions_API() {}

  virtual PP_Var GetProxyForURL(PP_Instance instance,
                                const std::string& url) = 0;
  virtual void UpdateActivity(PP_Instance instance) = 0;
  virtual PP_Bool SetCrashData(PP_Instance instance,
                               PP_FlashCrashKey key,
                               PP_Var value) = 0;
  virtual double GetLocalTimeZoneOffset(PP_Instance instance, PP_Time t) = 0;

  static const SingletonResourceID kSingletonResourceID = FLASH_SINGLETON_ID;
};

}  // namespace thunk
}  // namespace ppapi

#endif // PPAPI_THUNK_PPB_FLASH_FUNCTIONS_API_H_
