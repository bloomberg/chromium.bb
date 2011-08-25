// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// This is an early draft of background thread support.
// Until it is complete, we assume that all functions proxy functions
// (but CallOnMainThread) are called on the main thread.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_UPCALL_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_UPCALL_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"

namespace ppapi_proxy {

// Initialize the plugin side to allow upcalls (calls to the browser from
// other than the PPAPI main thread).
int32_t PluginUpcallStartup();

// Shut down the plugin side for upcalls.
void PluginUpcallShutdown();

// The upcall for PPB_Core.CallOnMainThread.
void PluginUpcallCoreCallOnMainThread(int32_t delay_in_milliseconds,
                                      PP_CompletionCallback callback,
                                      int32_t result);

// The upcall for PPB_Graphics2D.Flush.
int32_t PluginUpcallGraphics2DFlush(PP_Resource graphics_2d,
                                    PP_CompletionCallback callback);

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_UPCALL_H_
