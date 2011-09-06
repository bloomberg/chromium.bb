// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_SURFACE_3D_PROXY_H_
#define PPAPI_PPB_SURFACE_3D_PROXY_H_

#include <vector>

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_graphics_3d.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_surface_3d_api.h"

struct PPB_Surface3D_Dev;

namespace ppapi {
namespace proxy {

class Context3D;

class Surface3D : public ppapi::Resource,
                  public ppapi::thunk::PPB_Surface3D_API {
 public:
  explicit Surface3D(const ppapi::HostResource& host_resource);
  virtual ~Surface3D();

  // Resource overrides.
  virtual PPB_Surface3D_API* AsPPB_Surface3D_API() OVERRIDE;

  // PPB_Surface3D_API implementation.
  virtual int32_t SetAttrib(int32_t attribute, int32_t value) OVERRIDE;
  virtual int32_t GetAttrib(int32_t attribute, int32_t* value) OVERRIDE;
  virtual int32_t SwapBuffers(PP_CompletionCallback callback) OVERRIDE;

  void SwapBuffersACK(int32_t pp_error);

  bool is_flush_pending() const { return !!current_flush_callback_.func; }

  PP_CompletionCallback current_flush_callback() const {
    return current_flush_callback_;
  }

  void set_context(Context3D* context) {
    context_ = context;
  }

  Context3D* context() const { return context_; }

 private:
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

  static PP_Resource CreateProxyResource(PP_Instance instance,
                                         PP_Config3D_Dev config,
                                         const int32_t* attrib_list);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgCreate(PP_Instance instance,
                   PP_Config3D_Dev config,
                   const std::vector<int32_t>& attribs,
                   ppapi::HostResource* result);
  void OnMsgSwapBuffers(const ppapi::HostResource& surface);
  // Renderer->plugin message handlers.
  void OnMsgSwapBuffersACK(const ppapi::HostResource& surface,
                           int32_t pp_error);

  void SendSwapBuffersACKToPlugin(int32_t result,
                                  const ppapi::HostResource& surface_3d);

  pp::CompletionCallbackFactory<PPB_Surface3D_Proxy,
                                ProxyNonThreadSafeRefCount> callback_factory_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_SURFACE_3D_PROXY_H_
