// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_SURFACE_3D_PROXY_H_
#define PPAPI_PPB_SURFACE_3D_PROXY_H_

#include <vector>

#include "ppapi/c/dev/pp_graphics_3d_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"

struct PPB_Surface3D_Dev;

namespace pp {
namespace proxy {

class Context3D;

class Surface3D : public PluginResource {
 public:
  explicit Surface3D(const HostResource& host_resource)
      : PluginResource(host_resource),
        resource_(0),
        context_(NULL),
        current_flush_callback_(PP_BlockUntilComplete()) {
  }

  // Resource overrides.
  virtual Surface3D* AsSurface3D() { return this; }

  bool is_flush_pending() const { return !!current_flush_callback_.func; }

  PP_CompletionCallback current_flush_callback() const {
    return current_flush_callback_;
  }

  void set_current_flush_callback(PP_CompletionCallback cb) {
    current_flush_callback_ = cb;
  }

  void set_context(Context3D* context) {
    context_ = context;
  }

  Context3D* context() const { return context_; }

  void set_resource(PP_Resource resource) { resource_ = resource; }
  PP_Resource resource() const { return resource_; }

 private:
  PP_Resource resource_;
  Context3D* context_;
  // In the plugin, this is the current callback set for Flushes. When the
  // callback function pointer is non-NULL, we're waiting for a flush ACK.
  PP_CompletionCallback current_flush_callback_;

  DISALLOW_COPY_AND_ASSIGN(Surface3D);
};

class PPB_Surface3D_Proxy : public InterfaceProxy {
 public:
  PPB_Surface3D_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Surface3D_Proxy();

  static const Info* GetInfo();

  const PPB_Surface3D_Dev* ppb_surface_3d_target() const {
    return reinterpret_cast<const PPB_Surface3D_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgCreate(PP_Instance instance,
                   PP_Config3D_Dev config,
                   std::vector<int32_t> attribs,
                   HostResource* result);
  void OnMsgSwapBuffers(const HostResource& surface);
  // Renderer->plugin message handlers.
  void OnMsgSwapBuffersACK(const HostResource& surface, int32_t pp_error);

  void SendSwapBuffersACKToPlugin(int32_t result,
                                  const HostResource& surface_3d);

  CompletionCallbackFactory<PPB_Surface3D_Proxy,
                            ProxyNonThreadSafeRefCount> callback_factory_;
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_SURFACE_3D_PROXY_H_
