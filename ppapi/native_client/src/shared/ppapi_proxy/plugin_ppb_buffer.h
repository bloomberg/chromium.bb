// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_BUFFER_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_BUFFER_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_Buffer interface.
class PluginBuffer : public PluginResource {
 public:
  static const PPB_Buffer_Dev* GetInterface();

 protected:
  virtual ~PluginBuffer() {}

 private:
  IMPLEMENT_RESOURCE(PluginBuffer);
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginBuffer);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_BUFFER_H_
