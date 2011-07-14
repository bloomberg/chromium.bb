/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "native_client/src/trusted/plugin/portable_handle.h"
#include <stdio.h>
#include <string.h>
#include <map>

#include "native_client/src/trusted/plugin/browser_interface.h"

namespace plugin {

PortableHandle::PortableHandle() {
  PLUGIN_PRINTF(("PortableHandle::PortableHandle (this=%p)\n",
                 static_cast<void*>(this)));
}

PortableHandle::~PortableHandle() {
  PLUGIN_PRINTF(("PortableHandle::~PortableHandle (this=%p)\n",
                 static_cast<void*>(this)));
}

}  // namespace plugin
