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

PPB_Flash_Impl::PPB_Flash_Impl(PluginInstance* instance) {
}

PPB_Flash_Impl::~PPB_Flash_Impl() {
}

}  // namespace ppapi
}  // namespace webkit
