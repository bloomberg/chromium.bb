// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_surface_3d_proxy.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/dev/ppb_surface_3d_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_context_3d_proxy.h"

namespace pp {
namespace proxy {

namespace {

PP_Resource Create(PP_Instance instance,
                   PP_Config3D_Dev config,
                   const int32_t* attrib_list) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  std::vector<int32_t> attribs;
  if (attrib_list) {
    for (const int32_t* attr = attrib_list; attr; ++attr)
      attribs.push_back(*attr);
  } else {
    attribs.push_back(0);
  }

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBSurface3D_Create(
      INTERFACE_ID_PPB_SURFACE_3D, instance, config, attribs, &result));

  if (result.is_null())
    return 0;
  linked_ptr<Surface3D> surface_3d(new Surface3D(result));
  PP_Resource resource =
      PluginResourceTracker::GetInstance()->AddResource(surface_3d);
  surface_3d->set_resource(resource);
  return resource;
}

PP_Bool IsSurface3D(PP_Resource resource) {
  Surface3D* object = PluginResource::GetAs<Surface3D>(resource);
  return BoolToPPBool(!!object);
}

int32_t SetAttrib(PP_Resource surface_id,
                  int32_t attribute,
                  int32_t value) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t GetAttrib(PP_Resource surface_id,
                  int32_t attribute,
                  int32_t* value) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t SwapBuffers(PP_Resource surface_id,
                    PP_CompletionCallback callback) {
  Surface3D* object = PluginResource::GetAs<Surface3D>(surface_id);
  if (!object)
    return PP_ERROR_BADRESOURCE;
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      object->instance());
  if (!dispatcher)
    return PP_ERROR_FAILED;

  // For now, disallow blocking calls. We'll need to add support for other
  // threads to this later.
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  if (object->is_flush_pending())
    return PP_ERROR_INPROGRESS;  // Can't have >1 flush pending.

  if (!object->context())
    return PP_ERROR_FAILED;

  object->set_current_flush_callback(callback);

  IPC::Message* msg = new PpapiHostMsg_PPBSurface3D_SwapBuffers(
      INTERFACE_ID_PPB_SURFACE_3D, object->host_resource());
  msg->set_unblock(true);
  dispatcher->Send(msg);

  object->context()->gles2_impl()->SwapBuffers();

  return PP_ERROR_WOULDBLOCK;
}

const PPB_Surface3D_Dev ppb_surface_3d = {
  &Create,
  &IsSurface3D,
  &SetAttrib,
  &GetAttrib,
  &SwapBuffers
};

}  // namespace

PPB_Surface3D_Proxy::PPB_Surface3D_Proxy(Dispatcher* dispatcher,
                                         const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Surface3D_Proxy::~PPB_Surface3D_Proxy() {
}

const void* PPB_Surface3D_Proxy::GetSourceInterface() const {
  return &ppb_surface_3d;
}

InterfaceID PPB_Surface3D_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_SURFACE_3D;
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
                                      std::vector<int32_t> attribs,
                                      HostResource* result) {
  DCHECK(attribs.back() == 0);
  PP_Resource resource =
      ppb_surface_3d_target()->Create(instance, config, &attribs.front());
  result->SetHostResource(instance, resource);
}

void PPB_Surface3D_Proxy::OnMsgSwapBuffers(const HostResource& surface_3d) {
  CompletionCallback callback = callback_factory_.NewCallback(
      &PPB_Surface3D_Proxy::SendSwapBuffersACKToPlugin, surface_3d);
  int32_t result = ppb_surface_3d_target()->SwapBuffers(
      surface_3d.host_resource(), callback.pp_completion_callback());
  if (result != PP_ERROR_WOULDBLOCK) {
    // There was some error, so we won't get a flush callback. We need to now
    // issue the ACK to the plugin hears about the error. This will also clean
    // up the data associated with the callback.
    callback.Run(result);
  }
}

void PPB_Surface3D_Proxy::OnMsgSwapBuffersACK(const HostResource& resource,
                                              int32_t pp_error) {
  PP_Resource plugin_resource =
      PluginResourceTracker::GetInstance()->PluginResourceForHostResource(
          resource);
  if (!plugin_resource)
    return;
  Surface3D* object = PluginResource::GetAs<Surface3D>(plugin_resource);
  if (!object) {
    // The plugin has released the Surface3D object so don't issue the
    // callback.
    return;
  }

  // Be careful to make the callback NULL again before issuing the callback
  // since the plugin might want to flush from within the callback.
  PP_CompletionCallback callback = object->current_flush_callback();
  object->set_current_flush_callback(PP_BlockUntilComplete());
  PP_RunCompletionCallback(&callback, pp_error);
}

void PPB_Surface3D_Proxy::SendSwapBuffersACKToPlugin(
    int32_t result,
    const HostResource& surface_3d) {
  dispatcher()->Send(new PpapiMsg_PPBSurface3D_SwapBuffersACK(
      INTERFACE_ID_PPB_SURFACE_3D, surface_3d, result));
}

}  // namespace proxy
}  // namespace pp
