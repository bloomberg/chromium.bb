/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_INSTANCE_DATA_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_INSTANCE_DATA_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/native_client/src/shared/ppapi_proxy/view_data.h"

namespace ppapi_proxy {

// Per-instance data on the plugin side.
class PluginInstanceData {
 public:
  static PluginInstanceData* FromPP(PP_Instance id);
  static void DidCreate(PP_Instance id);
  static void DidDestroy(PP_Instance id);
  static bool IsFullscreen(PP_Instance id);

  PluginInstanceData(PP_Instance id) : id_(id) {
  }
  ~PluginInstanceData() {}

  PP_Instance id() { return id_; }

  void set_last_view_data(const ViewData& data) { last_view_data_ = data; }
  const ViewData& last_view_data() const { return last_view_data_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginInstanceData);

  PP_Instance id_;

  ViewData last_view_data_;
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_INSTANCE_DATA_H_
