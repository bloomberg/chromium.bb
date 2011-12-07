// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_INSTANCE_PROXY_H_
#define PPAPI_PROXY_PPB_INSTANCE_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/instance_impl.h"
#include "ppapi/thunk/ppb_instance_api.h"

// Windows headers interfere with this file.
#ifdef PostMessage
#undef PostMessage
#endif

namespace ppapi {
namespace proxy {

class SerializedVarReceiveInput;
class SerializedVarOutParam;
class SerializedVarReturnValue;

class PPB_Instance_Proxy : public InterfaceProxy,
                           public ppapi::InstanceImpl,
                           public ppapi::thunk::PPB_Instance_FunctionAPI {
 public:
  PPB_Instance_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Instance_Proxy();

  static const Info* GetInfoPrivate();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // FunctionGroupBase overrides.
  ppapi::thunk::PPB_Instance_FunctionAPI* AsPPB_Instance_FunctionAPI() OVERRIDE;

  // PPB_Instance_FunctionAPI implementation.
  virtual PP_Bool BindGraphics(PP_Instance instance,
                               PP_Resource device) OVERRIDE;
  virtual PP_Bool IsFullFrame(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetWindowObject(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetOwnerElementObject(PP_Instance instance) OVERRIDE;
  virtual PP_Var ExecuteScript(PP_Instance instance,
                               PP_Var script,
                               PP_Var* exception) OVERRIDE;
  virtual PP_Var GetDefaultCharSet(PP_Instance instance) OVERRIDE;
  virtual void Log(PP_Instance instance,
                   int log_level,
                   PP_Var value) OVERRIDE;
  virtual void LogWithSource(PP_Instance instance,
                             int log_level,
                             PP_Var source,
                             PP_Var value) OVERRIDE;
  virtual void NumberOfFindResultsChanged(PP_Instance instance,
                                          int32_t total,
                                          PP_Bool final_result) OVERRIDE;
  virtual void SelectedFindResultChanged(PP_Instance instance,
                                         int32_t index) OVERRIDE;
  virtual PP_Bool IsFullscreen(PP_Instance instance) OVERRIDE;
  virtual PP_Bool SetFullscreen(PP_Instance instance,
                                     PP_Bool fullscreen) OVERRIDE;
  virtual PP_Bool GetScreenSize(PP_Instance instance,
                                     PP_Size* size) OVERRIDE;
  virtual PP_Bool FlashIsFullscreen(PP_Instance instance) OVERRIDE;
  virtual PP_Bool FlashSetFullscreen(PP_Instance instance,
                                    PP_Bool fullscreen) OVERRIDE;
  virtual PP_Bool FlashGetScreenSize(PP_Instance instance, PP_Size* size)
      OVERRIDE;
  virtual int32_t RequestInputEvents(PP_Instance instance,
                                     uint32_t event_classes) OVERRIDE;
  virtual int32_t RequestFilteringInputEvents(PP_Instance instance,
                                              uint32_t event_classes) OVERRIDE;
  virtual void ClearInputEventRequest(PP_Instance instance,
                                      uint32_t event_classes) OVERRIDE;
  virtual void ZoomChanged(PP_Instance instance, double factor) OVERRIDE;
  virtual void ZoomLimitsChanged(PP_Instance instance,
                                 double minimum_factor,
                                 double maximium_factor) OVERRIDE;
  virtual PP_Var ResolveRelativeToDocument(
      PP_Instance instance,
      PP_Var relative,
      PP_URLComponents_Dev* components) OVERRIDE;
  virtual PP_Bool DocumentCanRequest(PP_Instance instance, PP_Var url) OVERRIDE;
  virtual PP_Bool DocumentCanAccessDocument(PP_Instance instance,
                                            PP_Instance target) OVERRIDE;
  virtual PP_Var GetDocumentURL(PP_Instance instance,
                                PP_URLComponents_Dev* components) OVERRIDE;
  virtual PP_Var GetPluginInstanceURL(
      PP_Instance instance,
      PP_URLComponents_Dev* components) OVERRIDE;
  virtual void PostMessage(PP_Instance instance, PP_Var message) OVERRIDE;
  virtual int32_t LockMouse(PP_Instance instance,
                            PP_CompletionCallback callback) OVERRIDE;
  virtual void UnlockMouse(PP_Instance instance) OVERRIDE;

  static const ApiID kApiID = API_ID_PPB_INSTANCE;

 private:
  // Plugin -> Host message handlers.
  void OnMsgGetWindowObject(PP_Instance instance,
                            SerializedVarReturnValue result);
  void OnMsgGetOwnerElementObject(PP_Instance instance,
                                  SerializedVarReturnValue result);
  void OnMsgBindGraphics(PP_Instance instance,
                         const ppapi::HostResource& device,
                         PP_Bool* result);
  void OnMsgIsFullFrame(PP_Instance instance, PP_Bool* result);
  void OnMsgExecuteScript(PP_Instance instance,
                          SerializedVarReceiveInput script,
                          SerializedVarOutParam out_exception,
                          SerializedVarReturnValue result);
  void OnMsgGetDefaultCharSet(PP_Instance instance,
                              SerializedVarReturnValue result);
  void OnMsgLog(PP_Instance instance,
                int log_level,
                SerializedVarReceiveInput value);
  void OnMsgLogWithSource(PP_Instance instance,
                          int log_level,
                          SerializedVarReceiveInput source,
                          SerializedVarReceiveInput value);
  void OnMsgSetFullscreen(PP_Instance instance,
                               PP_Bool fullscreen,
                               PP_Bool* result);
  void OnMsgGetScreenSize(PP_Instance instance,
                               PP_Bool* result,
                               PP_Size* size);
  void OnMsgFlashSetFullscreen(PP_Instance instance,
                               PP_Bool fullscreen,
                               PP_Bool* result);
  void OnMsgFlashGetScreenSize(PP_Instance instance,
                               PP_Bool* result,
                               PP_Size* size);
  void OnMsgRequestInputEvents(PP_Instance instance,
                               bool is_filtering,
                               uint32_t event_classes);
  void OnMsgClearInputEvents(PP_Instance instance,
                             uint32_t event_classes);
  void OnMsgPostMessage(PP_Instance instance,
                        SerializedVarReceiveInput message);
  void OnMsgLockMouse(PP_Instance instance);
  void OnMsgUnlockMouse(PP_Instance instance);
  void OnMsgResolveRelativeToDocument(PP_Instance instance,
                                      SerializedVarReceiveInput relative,
                                      SerializedVarReturnValue result);
  void OnMsgDocumentCanRequest(PP_Instance instance,
                               SerializedVarReceiveInput url,
                               PP_Bool* result);
  void OnMsgDocumentCanAccessDocument(PP_Instance active,
                                      PP_Instance target,
                                      PP_Bool* result);
  void OnMsgGetDocumentURL(PP_Instance instance,
                           SerializedVarReturnValue result);
  void OnMsgGetPluginInstanceURL(PP_Instance instance,
                                 SerializedVarReturnValue result);

  // Host -> Plugin message handlers.
  void OnMsgMouseLockComplete(PP_Instance instance, int32_t result);

  void MouseLockCompleteInHost(int32_t result, PP_Instance instance);

  pp::CompletionCallbackFactory<PPB_Instance_Proxy,
                                ProxyNonThreadSafeRefCount> callback_factory_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_INSTANCE_PROXY_H_
