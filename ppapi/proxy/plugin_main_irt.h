// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_MAIN_IRT_H_
#define PPAPI_PROXY_PLUGIN_MAIN_IRT_H_

#include "ppapi/nacl_irt/public/irt_ppapi.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

#ifdef __cplusplus
extern "C" {
#endif

// The entry point for the main thread of the PPAPI plugin process.
PPAPI_PROXY_EXPORT int PpapiPluginMain(void);

PPAPI_PROXY_EXPORT void PpapiPluginRegisterThreadCreator(
    const struct PP_ThreadFunctions* new_funcs);

// Sets the IPC channels for the browser and the renderer by the given FD
// numbers. This will be used for non-SFI mode. Must be called before
// PpapiPluginMain is called.
PPAPI_PROXY_EXPORT void SetIPCFileDescriptors(
    int browser_ipc_fd, int renderer_ipc_fd);

#ifdef __cplusplus
}
#endif

#endif  // PPAPI_PROXY_PLUGIN_MAIN_IRT_H_
