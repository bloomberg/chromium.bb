// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_INSTANCE_PROXY_H_
#define PPAPI_PROXY_PPB_INSTANCE_PROXY_H_

#include <string>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_completion_callback_factory.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/ppb_instance_shared.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/utility/completion_callback_factory.h"

// Windows headers interfere with this file.
#ifdef PostMessage
#undef PostMessage
#endif

struct PP_DecryptedBlockInfo;

namespace ppapi {
namespace proxy {

class SerializedVarReceiveInput;
class SerializedVarOutParam;
class SerializedVarReturnValue;

class PPB_Instance_Proxy : public InterfaceProxy,
                           public PPB_Instance_Shared {
 public:
  PPB_Instance_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Instance_Proxy();

  static const Info* GetInfoPrivate();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // PPB_Instance_API implementation.
  virtual PP_Bool BindGraphics(PP_Instance instance,
                               PP_Resource device) OVERRIDE;
  virtual PP_Bool IsFullFrame(PP_Instance instance) OVERRIDE;
  virtual const ViewData* GetViewData(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetWindowObject(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetOwnerElementObject(PP_Instance instance) OVERRIDE;
  virtual PP_Var ExecuteScript(PP_Instance instance,
                               PP_Var script,
                               PP_Var* exception) OVERRIDE;
  virtual uint32_t GetAudioHardwareOutputSampleRate(PP_Instance instance)
      OVERRIDE;
  virtual uint32_t GetAudioHardwareOutputBufferSize(PP_Instance instance)
      OVERRIDE;
  virtual PP_Var GetDefaultCharSet(PP_Instance instance) OVERRIDE;
  virtual void NumberOfFindResultsChanged(PP_Instance instance,
                                          int32_t total,
                                          PP_Bool final_result) OVERRIDE;
  virtual void SelectedFindResultChanged(PP_Instance instance,
                                         int32_t index) OVERRIDE;
  virtual PP_Var GetFontFamilies(PP_Instance instance) OVERRIDE;
  virtual PP_Bool SetFullscreen(PP_Instance instance,
                                PP_Bool fullscreen) OVERRIDE;
  virtual PP_Bool GetScreenSize(PP_Instance instance,
                                PP_Size* size) OVERRIDE;
  virtual thunk::PPB_Flash_API* GetFlashAPI() OVERRIDE;
  virtual thunk::PPB_Gamepad_API* GetGamepadAPI(PP_Instance instance) OVERRIDE;
  virtual int32_t RequestInputEvents(PP_Instance instance,
                                     uint32_t event_classes) OVERRIDE;
  virtual int32_t RequestFilteringInputEvents(PP_Instance instance,
                                              uint32_t event_classes) OVERRIDE;
  virtual void ClearInputEventRequest(PP_Instance instance,
                                      uint32_t event_classes) OVERRIDE;
  virtual void ClosePendingUserGesture(PP_Instance instance,
                                       PP_TimeTicks timestamp) OVERRIDE;
  virtual void ZoomChanged(PP_Instance instance, double factor) OVERRIDE;
  virtual void ZoomLimitsChanged(PP_Instance instance,
                                 double minimum_factor,
                                 double maximium_factor) OVERRIDE;
  virtual void PostMessage(PP_Instance instance, PP_Var message) OVERRIDE;
  virtual PP_Bool SetCursor(PP_Instance instance,
                            PP_MouseCursor_Type type,
                            PP_Resource image,
                            const PP_Point* hot_spot) OVERRIDE;
  virtual int32_t LockMouse(PP_Instance instance,
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void UnlockMouse(PP_Instance instance) OVERRIDE;
  virtual void SetTextInputType(PP_Instance instance,
                                PP_TextInput_Type type) OVERRIDE;
  virtual void UpdateCaretPosition(PP_Instance instance,
                                   const PP_Rect& caret,
                                   const PP_Rect& bounding_box) OVERRIDE;
  virtual void CancelCompositionText(PP_Instance instance) OVERRIDE;
  virtual void SelectionChanged(PP_Instance instance) OVERRIDE;
  virtual void UpdateSurroundingText(PP_Instance instance,
                                     const char* text,
                                     uint32_t caret,
                                     uint32_t anchor) OVERRIDE;

#if !defined(OS_NACL)
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
  virtual void NeedKey(PP_Instance instance,
                       PP_Var key_system,
                       PP_Var session_id,
                       PP_Var init_data) OVERRIDE;
  virtual void KeyAdded(PP_Instance instance,
                        PP_Var key_system,
                        PP_Var session_id) OVERRIDE;
  virtual void KeyMessage(PP_Instance instance,
                          PP_Var key_system,
                          PP_Var session_id,
                          PP_Resource message,
                          PP_Var default_url) OVERRIDE;
  virtual void KeyError(PP_Instance instance,
                        PP_Var key_system,
                        PP_Var session_id,
                        int32_t media_error,
                        int32_t system_code) OVERRIDE;
  virtual void DeliverBlock(PP_Instance instance,
                            PP_Resource decrypted_block,
                            const PP_DecryptedBlockInfo* block_info) OVERRIDE;
  virtual void DeliverFrame(PP_Instance instance,
                            PP_Resource decrypted_frame,
                            const PP_DecryptedBlockInfo* block_info) OVERRIDE;
  virtual void DeliverSamples(PP_Instance instance,
                              PP_Resource decrypted_samples,
                              const PP_DecryptedBlockInfo* block_info) OVERRIDE;
#endif  // !defined(OS_NACL)

  static const ApiID kApiID = API_ID_PPB_INSTANCE;

 private:
  // Plugin -> Host message handlers.
  void OnHostMsgGetWindowObject(PP_Instance instance,
                                SerializedVarReturnValue result);
  void OnHostMsgGetOwnerElementObject(PP_Instance instance,
                                      SerializedVarReturnValue result);
  void OnHostMsgBindGraphics(PP_Instance instance,
                             const ppapi::HostResource& device,
                             PP_Bool* result);
  void OnHostMsgIsFullFrame(PP_Instance instance, PP_Bool* result);
  void OnHostMsgExecuteScript(PP_Instance instance,
                              SerializedVarReceiveInput script,
                              SerializedVarOutParam out_exception,
                              SerializedVarReturnValue result);
  void OnHostMsgGetAudioHardwareOutputSampleRate(PP_Instance instance,
                                                 uint32_t *result);
  void OnHostMsgGetAudioHardwareOutputBufferSize(PP_Instance instance,
                                                 uint32_t *result);
  void OnHostMsgGetDefaultCharSet(PP_Instance instance,
                                  SerializedVarReturnValue result);
  void OnHostMsgSetFullscreen(PP_Instance instance,
                              PP_Bool fullscreen,
                              PP_Bool* result);
  void OnHostMsgGetScreenSize(PP_Instance instance,
                              PP_Bool* result,
                              PP_Size* size);
  void OnHostMsgRequestInputEvents(PP_Instance instance,
                                   bool is_filtering,
                                   uint32_t event_classes);
  void OnHostMsgClearInputEvents(PP_Instance instance,
                                 uint32_t event_classes);
  void OnMsgHandleInputEventAck(PP_Instance instance,
                                PP_TimeTicks timestamp);
  void OnHostMsgPostMessage(PP_Instance instance,
                            SerializedVarReceiveInput message);
  void OnHostMsgLockMouse(PP_Instance instance);
  void OnHostMsgUnlockMouse(PP_Instance instance);
  void OnHostMsgSetCursor(PP_Instance instance,
                          int32_t type,
                          const ppapi::HostResource& custom_image,
                          const PP_Point& hot_spot);
  void OnHostMsgSetTextInputType(PP_Instance instance, PP_TextInput_Type type);
  void OnHostMsgUpdateCaretPosition(PP_Instance instance,
                                    const PP_Rect& caret,
                                    const PP_Rect& bounding_box);
  void OnHostMsgCancelCompositionText(PP_Instance instance);
  void OnHostMsgUpdateSurroundingText(
      PP_Instance instance,
      const std::string& text,
      uint32_t caret,
      uint32_t anchor);

#if !defined(OS_NACL)
  void OnHostMsgResolveRelativeToDocument(PP_Instance instance,
                                          SerializedVarReceiveInput relative,
                                          SerializedVarReturnValue result);
  void OnHostMsgDocumentCanRequest(PP_Instance instance,
                                   SerializedVarReceiveInput url,
                                   PP_Bool* result);
  void OnHostMsgDocumentCanAccessDocument(PP_Instance active,
                                          PP_Instance target,
                                          PP_Bool* result);
  void OnHostMsgGetDocumentURL(PP_Instance instance,
                               SerializedVarReturnValue result);
  void OnHostMsgGetPluginInstanceURL(PP_Instance instance,
                                     SerializedVarReturnValue result);
  virtual void OnHostMsgNeedKey(PP_Instance instance,
                                SerializedVarReceiveInput key_system,
                                SerializedVarReceiveInput session_id,
                                SerializedVarReceiveInput init_data);
  virtual void OnHostMsgKeyAdded(PP_Instance instance,
                                 SerializedVarReceiveInput key_system,
                                 SerializedVarReceiveInput session_id);
  virtual void OnHostMsgKeyMessage(PP_Instance instance,
                                   SerializedVarReceiveInput key_system,
                                   SerializedVarReceiveInput session_id,
                                   PP_Resource message,
                                   SerializedVarReceiveInput default_url);
  virtual void OnHostMsgKeyError(PP_Instance instance,
                                 SerializedVarReceiveInput key_system,
                                 SerializedVarReceiveInput session_id,
                                 int32_t media_error,
                                 int32_t system_code);
  virtual void OnHostMsgDeliverBlock(PP_Instance instance,
                                     PP_Resource decrypted_block,
                                     const std::string& serialized_block_info);
  virtual void OnHostMsgDeliverFrame(PP_Instance instance,
                                     PP_Resource decrypted_frame,
                                     const std::string& serialized_block_info);
  virtual void OnHostMsgDeliverSamples(
      PP_Instance instance,
      PP_Resource decrypted_samples,
      const std::string& serialized_block_info);
#endif  // !defined(OS_NACL)

  // Host -> Plugin message handlers.
  void OnPluginMsgMouseLockComplete(PP_Instance instance, int32_t result);

  void MouseLockCompleteInHost(int32_t result, PP_Instance instance);

  // Other helpers.
  void CancelAnyPendingRequestSurroundingText(PP_Instance instance);

  ProxyCompletionCallbackFactory<PPB_Instance_Proxy> callback_factory_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_INSTANCE_PROXY_H_
