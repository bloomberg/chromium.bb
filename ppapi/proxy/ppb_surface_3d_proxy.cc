// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_surface_3d_proxy.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/dev/ppb_surface_3d_dev.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_context_3d_proxy.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterFunctionNoLock;
using ppapi::thunk::PPB_Surface3D_API;
using ppapi::thunk::ResourceCreationAPI;

namespace ppapi {
namespace proxy {

namespace {

InterfaceProxy* CreateSurface3DProxy(Dispatcher* dispatcher) {
  return new PPB_Surface3D_Proxy(dispatcher);
}

}  // namespace

// Surface3D -------------------------------------------------------------------

Surface3D::Surface3D(const HostResource& host_resource)
    : Resource(host_resource),
      context_(NULL),
      current_flush_callback_(PP_BlockUntilComplete()) {
}

Surface3D::~Surface3D() {
  if (context_)
    context_->BindSurfaces(0, 0);
}

PPB_Surface3D_API* Surface3D::AsPPB_Surface3D_API() {
  return this;
}

int32_t Surface3D::SetAttrib(int32_t attribute, int32_t value) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t Surface3D::GetAttrib(int32_t attribute, int32_t* value) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t Surface3D::SwapBuffers(PP_CompletionCallback callback) {
  // For now, disallow blocking calls. We'll need to add support for other
  // threads to this later.
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  if (is_flush_pending())
    return PP_ERROR_INPROGRESS;  // Can't have >1 flush pending.

  if (!context_)
    return PP_ERROR_FAILED;

  current_flush_callback_ = callback;

  IPC::Message* msg = new PpapiHostMsg_PPBSurface3D_SwapBuffers(
      INTERFACE_ID_PPB_SURFACE_3D, host_resource());
  msg->set_unblock(true);
  PluginDispatcher::GetForResource(this)->Send(msg);

  context_->gles2_impl()->SwapBuffers();
  return PP_OK_COMPLETIONPENDING;
}

void Surface3D::SwapBuffersACK(int32_t pp_error) {
  PP_RunAndClearCompletionCallback(&current_flush_callback_, pp_error);
}

// PPB_Surface3D_Proxy ---------------------------------------------------------

PPB_Surface3D_Proxy::PPB_Surface3D_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Surface3D_Proxy::~PPB_Surface3D_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Surface3D_Proxy::GetInfo() {
  static const Info info = {
    thunk::GetPPB_Surface3D_Dev_Thunk(),
    PPB_SURFACE_3D_DEV_INTERFACE,
    INTERFACE_ID_PPB_SURFACE_3D,
    false,
    &CreateSurface3DProxy,
  };
  return &info;
}

// static
PP_Resource PPB_Surface3D_Proxy::CreateProxyResource(
    PP_Instance instance,
    PP_Config3D_Dev config,
    const int32_t* attrib_list) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  std::vector<int32_t> attribs;
  if (attrib_list) {
    const int32_t* attr = attrib_list;
    while(*attr != PP_GRAPHICS3DATTRIB_NONE) {
      attribs.push_back(*(attr++));  // Attribute.
      attribs.push_back(*(attr++));  // Value.
    }
  }
  attribs.push_back(PP_GRAPHICS3DATTRIB_NONE);  // Always terminate.

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBSurface3D_Create(
      INTERFACE_ID_PPB_SURFACE_3D, instance, config, attribs, &result));

  if (result.is_null())
    return 0;
  return (new Surface3D(result))->GetReference();
}

bool PPB_Surface3D_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Surface3D_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBSurface3D_Create,
                        OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBSurface3D_SwapBuffers,
                        OnMsgSwapBuffers)

    IPC_MESSAGE_HANDLER(PpapiMsg_PPBSurface3D_SwapBuffersACK,
                        OnMsgSwapBuffersACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // FIXME(brettw) handle bad messages!
  return handled;
}

void PPB_Surface3D_Proxy::OnMsgCreate(PP_Instance instance,
                                      PP_Config3D_Dev config,
                                      const std::vector<int32_t>& attribs,
                                      HostResource* result) {
  if (attribs.empty() ||
      attribs.size() % 2 != 1 ||
      attribs.back() != PP_GRAPHICS3DATTRIB_NONE)
    return;  // Bad message.

  thunk::EnterResourceCreation enter(instance);
  if (enter.succeeded()) {
    result->SetHostResource(
        instance,
        enter.functions()->CreateSurface3D(instance, config, &attribs.front()));
  }
}

void PPB_Surface3D_Proxy::OnMsgSwapBuffers(const HostResource& surface_3d) {
  EnterHostFromHostResourceForceCallback<PPB_Surface3D_API> enter(
      surface_3d, callback_factory_,
      &PPB_Surface3D_Proxy::SendSwapBuffersACKToPlugin, surface_3d);
  if (enter.succeeded())
    enter.SetResult(enter.object()->SwapBuffers(enter.callback()));
}

void PPB_Surface3D_Proxy::OnMsgSwapBuffersACK(const HostResource& resource,
                                              int32_t pp_error) {
  EnterPluginFromHostResource<PPB_Surface3D_API> enter(resource);
  if (enter.succeeded())
    static_cast<Surface3D*>(enter.object())->SwapBuffersACK(pp_error);
}

void PPB_Surface3D_Proxy::SendSwapBuffersACKToPlugin(
    int32_t result,
    const HostResource& surface_3d) {
  dispatcher()->Send(new PpapiMsg_PPBSurface3D_SwapBuffersACK(
      INTERFACE_ID_PPB_SURFACE_3D, surface_3d, result));
}

}  // namespace proxy
}  // namespace ppapi
