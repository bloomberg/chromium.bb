/*
  Copyright (c) 2011 The Chromium Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_NACL_FILE_H
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_NACL_FILE_H

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"

namespace ppapi_proxy {

// Loads |url| asynchronously notifying the |callback| of the final result.
// If the call cannot be completed asynchronously, |callback|
// will not be invoked. Returns one of the PP_ERROR codes.
int32_t StreamAsFile(PP_Instance instance,
                     const char* url,
                     struct PP_CompletionCallback callback);

// Returns an open file descriptor for a |url| loaded using StreamAsFile()
// or -1 if it has not been loaded.
int GetFileDesc(PP_Instance instance,
                const char* url);

}  // namespace ppapi_proxy

#endif /*  NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_NACL_FILE_H */
