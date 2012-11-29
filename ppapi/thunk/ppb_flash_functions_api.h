// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FLASH_FUNCTIONS_API_H_
#define PPAPI_THUNK_PPB_FLASH_FUNCTIONS_API_H_

#include "ppapi/shared_impl/singleton_resource_id.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

struct PP_ArrayOutput;

namespace ppapi {
namespace thunk {

// This class collects all of the Flash interface-related APIs into one place.
// PPB_Flash_API is deprecated in favor of this (the new resource model uses
// this API).
class PPAPI_THUNK_EXPORT PPB_Flash_Functions_API {
 public:
  virtual ~PPB_Flash_Functions_API() {}

  static const SingletonResourceID kSingletonResourceID = FLASH_SINGLETON_ID;
};

}  // namespace thunk
}  // namespace ppapi

#endif // PPAPI_THUNK_PPB_FLASH_FUNCTIONS_API_H_
