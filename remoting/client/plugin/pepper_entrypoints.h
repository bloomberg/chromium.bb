// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_ENTRYPOINTS_
#define REMOTING_CLIENT_PLUGIN_PEPPER_ENTRYPOINTS_

#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"

namespace remoting {

int PPP_InitializeModule(PP_Module module_id,
                         PPB_GetInterface get_browser_interface);
void PPP_ShutdownModule();
const void* PPP_GetInterface(const char* interface_name);
const void* PPP_GetBrowserInterface(const char* interface_name);

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_ENTRYPOINTS_
