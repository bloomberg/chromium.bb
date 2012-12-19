// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "ppapi/thunk/ppb_flash_api.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_Flash_Impl : public ::ppapi::thunk::PPB_Flash_API {
 public:
  explicit PPB_Flash_Impl(PluginInstance* instance);
  virtual ~PPB_Flash_Impl();

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_
