/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/desc_based_handle.h"
#include "native_client/src/trusted/plugin/plugin.h"


namespace plugin {

DescBasedHandle::DescBasedHandle(Plugin* plugin, nacl::DescWrapper* wrapper)
    : plugin_(plugin), wrapper_(wrapper) {
  PLUGIN_PRINTF(("DescBasedHandle::DescBasedHandle (this=%p)\n",
                 static_cast<void*>(this)));
}

DescBasedHandle::~DescBasedHandle() {
  PLUGIN_PRINTF(("DescBasedHandle::~DescBasedHandle (this=%p)\n",
                 static_cast<void*>(this)));
}

DescBasedHandle* DescBasedHandle::New(Plugin* plugin,
                                      nacl::DescWrapper* wrapper) {
  PLUGIN_PRINTF(("DescBasedHandle::New (plugin=%p)\n",
                 static_cast<void*>(plugin)));

  if ((plugin == NULL) || (wrapper == NULL)) {
    return NULL;
  }
  return new DescBasedHandle(plugin, wrapper);
}

BrowserInterface* DescBasedHandle::browser_interface() const {
  return plugin_->browser_interface();
}

}  // namespace plugin
