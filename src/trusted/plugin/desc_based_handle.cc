/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <stdio.h>
#include <string.h>

#include <map>

#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/desc_based_handle.h"
#include "native_client/src/trusted/plugin/portable_handle.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/plugin.h"


namespace plugin {

DescBasedHandle::DescBasedHandle()
    : plugin_(NULL), wrapper_(NULL) {
  PLUGIN_PRINTF(("DescBasedHandle::DescBasedHandle (this=%p)\n",
                 static_cast<void*>(this)));
}

DescBasedHandle::~DescBasedHandle() {
  PLUGIN_PRINTF(("DescBasedHandle::~DescBasedHandle (this=%p)\n",
                 static_cast<void*>(this)));
  if (NULL != wrapper_) {
    delete wrapper_;
    wrapper_ = NULL;
  }
}

DescBasedHandle* DescBasedHandle::New(Plugin* plugin,
                                      nacl::DescWrapper* wrapper) {
  PLUGIN_PRINTF(("DescBasedHandle::New (plugin=%p)\n",
                 static_cast<void*>(plugin)));

  DescBasedHandle* desc_based_handle = new(std::nothrow) DescBasedHandle();

  if (desc_based_handle == NULL || !desc_based_handle->Init(plugin, wrapper)) {
    return NULL;
  }

  return desc_based_handle;
}

bool DescBasedHandle::Init(Plugin* plugin, nacl::DescWrapper* wrapper) {
  plugin_ = plugin;
  wrapper_ = wrapper;
  return true;
}

BrowserInterface* DescBasedHandle::browser_interface() const {
  return plugin_->browser_interface();
}

}  // namespace plugin
