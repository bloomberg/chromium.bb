// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_

#include "base/basictypes.h"
#include "build/build_config.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/private/ppb_flash.h"

namespace webkit {
namespace ppapi {

class PPB_Flash_Impl {
 public:
  // Returns a pointer to the interface implementing PPB_Flash that is
  // exposed to the plugin.
  static const PPB_Flash_11* GetInterface11();
  static const PPB_Flash_12_0* GetInterface12_0();

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_
