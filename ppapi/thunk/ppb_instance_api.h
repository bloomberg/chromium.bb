// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_INSTANCE_API_H_
#define PPAPI_THUNK_INSTANCE_API_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/c/dev/ppb_text_input_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_mouse_cursor.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/shared_impl/api_id.h"

// Windows headers interfere with this file.
#ifdef PostMessage
#undef PostMessage
#endif

struct PP_DecryptedBlockInfo;

namespace ppapi {

class TrackedCallback;
struct ViewData;

namespace thunk {

class PPB_Flash_API;
class PPB_Gamepad_API;

class PPB_Instance_API {
 public:
  virtual ~PPB_Instance_API() {}

  virtual PP_Bool BindGraphics(PP_Instance instance, PP_Resource device) = 0;
  virtual PP_Bool IsFullFrame(PP_Instance instance) = 0;

  // Not an exposed PPAPI function, this returns the internal view data struct.
  virtual const ViewData* GetViewData(PP_Instance instance) = 0;

  // InstancePrivate.
  virtual PP_Var GetWindowObject(PP_Instance instance) = 0;
  virtual PP_Var GetOwnerElementObject(PP_Instance instance) = 0;
  virtual PP_Var ExecuteScript(PP_Instance instance,
                               PP_Var script,
                               PP_Var* exception) = 0;

  // Audio.
  virtual uint32_t GetAudioHardwareOutputSampleRate(PP_Instance instance) = 0;
  virtual uint32_t GetAudioHardwareOutputBufferSize(PP_Instance instance) = 0;

  // CharSet.
  virtual PP_Var GetDefaultCharSet(PP_Instance instance) = 0;

  // Console.
  virtual void Log(PP_Instance instance,
                   PP_LogLevel_Dev log_level,
                   PP_Var value) = 0;
  virtual void LogWithSource(PP_Instance instance,
                             PP_LogLevel_Dev log_level,
                             PP_Var source,
                             PP_Var value) = 0;

  // Find.
  virtual void NumberOfFindResultsChanged(PP_Instance instance,
                                          int32_t total,
                                          PP_Bool final_result) = 0;
  virtual void SelectedFindResultChanged(PP_Instance instance,
                                         int32_t index) = 0;

  // Font.
  virtual PP_Var GetFontFamilies(PP_Instance instance) = 0;

  // Fullscreen.
  virtual PP_Bool SetFullscreen(PP_Instance instance,
                                PP_Bool fullscreen) = 0;
  virtual PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size) = 0;

  // Flash.
  virtual PPB_Flash_API* GetFlashAPI() = 0;

  // Gamepad.
  virtual PPB_Gamepad_API* GetGamepadAPI(PP_Instance instance) = 0;

  // InputEvent.
  virtual int32_t RequestInputEvents(PP_Instance instance,
                                     uint32_t event_classes) = 0;
  virtual int32_t RequestFilteringInputEvents(PP_Instance instance,
                                              uint32_t event_classes) = 0;
  virtual void ClearInputEventRequest(PP_Instance instance,
                                      uint32_t event_classes) = 0;
  virtual void ClosePendingUserGesture(PP_Instance instance,
                                       PP_TimeTicks timestamp) = 0;

  // Messaging.
  virtual void PostMessage(PP_Instance instance, PP_Var message) = 0;

  // Mouse cursor.
  virtual PP_Bool SetCursor(PP_Instance instance,
                            PP_MouseCursor_Type type,
                            PP_Resource image,
                            const PP_Point* hot_spot) = 0;

  // MouseLock.
  virtual int32_t LockMouse(PP_Instance instance,
                            scoped_refptr<TrackedCallback> callback) = 0;
  virtual void UnlockMouse(PP_Instance instance) = 0;

  // TextInput.
  virtual void SetTextInputType(PP_Instance instance,
                                PP_TextInput_Type type) = 0;
  virtual void UpdateCaretPosition(PP_Instance instance,
                                   const PP_Rect& caret,
                                   const PP_Rect& bounding_box) = 0;
  virtual void CancelCompositionText(PP_Instance instance) = 0;
  virtual void SelectionChanged(PP_Instance instance) = 0;
  virtual void UpdateSurroundingText(PP_Instance instance,
                                     const char* text,
                                     uint32_t caret,
                                     uint32_t anchor) = 0;

  // Zoom.
  virtual void ZoomChanged(PP_Instance instance, double factor) = 0;
  virtual void ZoomLimitsChanged(PP_Instance instance,
                                 double minimum_factor,
                                 double maximium_factor) = 0;
#if !defined(OS_NACL)
  // Content Decryptor.
  virtual void NeedKey(PP_Instance instance,
                       PP_Var key_system,
                       PP_Var session_id,
                       PP_Var init_data) = 0;
  virtual void KeyAdded(PP_Instance instance,
                        PP_Var key_system,
                        PP_Var session_id) = 0;
  virtual void KeyMessage(PP_Instance instance,
                          PP_Var key_system,
                          PP_Var session_id,
                          PP_Resource message,
                          PP_Var default_url) = 0;
  virtual void KeyError(PP_Instance instance,
                        PP_Var key_system,
                        PP_Var session_id,
                        int32_t media_error,
                        int32_t system_error) = 0;
  virtual void DeliverBlock(PP_Instance instance,
                            PP_Resource decrypted_block,
                            const PP_DecryptedBlockInfo* block_info) = 0;
  virtual void DeliverFrame(PP_Instance instance,
                            PP_Resource decrypted_frame,
                            const PP_DecryptedBlockInfo* block_info) = 0;
  virtual void DeliverSamples(PP_Instance instance,
                              PP_Resource decrypted_samples,
                              const PP_DecryptedBlockInfo* block_info) = 0;

  // URLUtil.
  virtual PP_Var ResolveRelativeToDocument(
      PP_Instance instance,
      PP_Var relative,
      PP_URLComponents_Dev* components) = 0;
  virtual PP_Bool DocumentCanRequest(PP_Instance instance, PP_Var url) = 0;
  virtual PP_Bool DocumentCanAccessDocument(PP_Instance instance,
                                            PP_Instance target) = 0;
  virtual PP_Var GetDocumentURL(PP_Instance instance,
                                PP_URLComponents_Dev* components) = 0;
  virtual PP_Var GetPluginInstanceURL(PP_Instance instance,
                                      PP_URLComponents_Dev* components) = 0;
#endif  // !defined(OS_NACL)

  static const ApiID kApiID = API_ID_PPB_INSTANCE;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_INSTANCE_API_H_
