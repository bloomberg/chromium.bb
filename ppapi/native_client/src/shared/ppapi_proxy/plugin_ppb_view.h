// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_VIEW_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_VIEW_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/view_data.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "ppapi/c/ppb_view.h"

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_View interface.
class PluginView : public PluginResource {
 public:
  PluginView();
  void Init(const ViewData& view_data);

  // PluginResource implementation.
  virtual bool InitFromBrowserResource(PP_Resource /*resource*/) {
    return true;
  }

  const ViewData& view_data() const { return view_data_; }

  static const PPB_View* GetInterface();

 private:
  virtual ~PluginView();

  ViewData view_data_;

  IMPLEMENT_RESOURCE(PluginView);
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginView);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_VIEW_H_

