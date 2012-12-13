// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_flash_impl.h"

#include <string>

#include "googleurl/src/gurl.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::PPTimeToTime;
using ppapi::StringVar;
using ppapi::thunk::EnterResourceNoLock;

namespace webkit {
namespace ppapi {

PPB_Flash_Impl::PPB_Flash_Impl(PluginInstance* instance)
    : instance_(instance) {
}

PPB_Flash_Impl::~PPB_Flash_Impl() {
}

double PPB_Flash_Impl::GetLocalTimeZoneOffset(PP_Instance instance,
                                              PP_Time t) {
  // Evil hack. The time code handles exact "0" values as special, and produces
  // a "null" Time object. This will represent some date hundreds of years ago
  // and will give us funny results at 1970 (there are some tests where this
  // comes up, but it shouldn't happen in real life). To work around this
  // special handling, we just need to give it some nonzero value.
  if (t == 0.0)
    t = 0.0000000001;

  // We can't do the conversion here because on Linux, the localtime calls
  // require filesystem access prohibited by the sandbox.
  return instance_->delegate()->GetLocalTimeZoneOffset(PPTimeToTime(t));
}

PP_Var PPB_Flash_Impl::GetSetting(PP_Instance instance,
                                  PP_FlashSetting setting) {
  switch(setting) {
    case PP_FLASHSETTING_LSORESTRICTIONS: {
      return PP_MakeInt32(
          instance_->delegate()->GetLocalDataRestrictions(
              instance_->container()->element().document().url(),
              instance_->plugin_url()));
    }
    default:
      // No other settings are supported in-process.
      return PP_MakeUndefined();
  }
}

}  // namespace ppapi
}  // namespace webkit
