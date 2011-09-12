/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/desc_based_handle.h"
#include "native_client/src/trusted/plugin/plugin.h"


namespace plugin {

DescBasedHandle::DescBasedHandle(nacl::DescWrapper* wrapper)
    : wrapper_(wrapper) {
  PLUGIN_PRINTF(("DescBasedHandle::DescBasedHandle (this=%p)\n",
                 static_cast<void*>(this)));
}

DescBasedHandle::~DescBasedHandle() {
  PLUGIN_PRINTF(("DescBasedHandle::~DescBasedHandle (this=%p)\n",
                 static_cast<void*>(this)));
}

DescBasedHandle* DescBasedHandle::New(nacl::DescWrapper* wrapper) {
  PLUGIN_PRINTF(("DescBasedHandle::New (wrapper=%p)\n",
                 static_cast<void*>(wrapper)));

  if (wrapper == NULL) {
    return NULL;
  }
  return new DescBasedHandle(wrapper);
}

}  // namespace plugin
