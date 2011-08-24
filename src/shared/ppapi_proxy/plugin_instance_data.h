/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_INSTANCE_DATA_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_INSTANCE_DATA_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_rect.h"

namespace ppapi_proxy {

// Per-instance data on the plugin side.
class PluginInstanceData {
 public:
  static PluginInstanceData* FromPP(PP_Instance id);
  static void DidCreate(PP_Instance id);
  static void DidDestroy(PP_Instance id);
  static void DidChangeView(PP_Instance id, PP_Rect position, PP_Rect clip);

  PluginInstanceData(PP_Instance id)
    : id_(id), position_(PP_MakeRectFromXYWH(0, 0, 0, 0)) {
  }
  ~PluginInstanceData() {}

  PP_Instance id() { return id_; }
  PP_Rect position() { return position_; }
  void set_position(PP_Rect position) { position_ = position; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginInstanceData);

  PP_Instance id_;
  PP_Rect position_;
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_INSTANCE_DATA_H_
