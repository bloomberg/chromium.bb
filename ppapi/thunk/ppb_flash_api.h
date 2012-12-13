// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FLASH_API_H_
#define PPAPI_THUNK_PPB_FLASH_API_H_

#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

/////////////////////////// WARNING:DEPRECTATED ////////////////////////////////
// Please do not add any new functions to this API. They should be implemented
// in the new-style resource proxy (see flash_functions_api.h and
// flash_resource.h).
// TODO(raymes): All of these functions should be moved to
// flash_functions_api.h.
////////////////////////////////////////////////////////////////////////////////
// This class collects all of the Flash interface-related APIs into one place.
class PPAPI_THUNK_EXPORT PPB_Flash_API {
 public:
  virtual ~PPB_Flash_API() {}

  // Flash.
  virtual double GetLocalTimeZoneOffset(PP_Instance instance, PP_Time t) = 0;
  virtual PP_Var GetSetting(PP_Instance instance, PP_FlashSetting setting) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif // PPAPI_THUNK_PPB_FLASH_API_H_
