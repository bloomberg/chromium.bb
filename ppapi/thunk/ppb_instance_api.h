// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_INSTANCE_API_H_
#define PPAPI_THUNK_INSTANCE_API_H_

#include "ppapi/c/dev/ppb_gamepad_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/shared_impl/api_id.h"

// Windows headers interfere with this file.
#ifdef PostMessage
#undef PostMessage
#endif

namespace ppapi {

struct ViewData;

namespace thunk {

class PPB_Instance_FunctionAPI {
 public:
  virtual ~PPB_Instance_FunctionAPI() {}

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

  // CharSet.
  virtual PP_Var GetDefaultCharSet(PP_Instance instance) = 0;

  // Console.
  virtual void Log(PP_Instance instance,
                   int log_level,
                   PP_Var value) = 0;
  virtual void LogWithSource(PP_Instance instance,
                             int log_level,
                             PP_Var source,
                             PP_Var value) = 0;

  // Find.
  virtual void NumberOfFindResultsChanged(PP_Instance instance,
                                          int32_t total,
                                          PP_Bool final_result) = 0;
  virtual void SelectedFindResultChanged(PP_Instance instance,
                                         int32_t index) = 0;

  // Fullscreen.
  virtual PP_Bool SetFullscreen(PP_Instance instance,
                                PP_Bool fullscreen) = 0;
  virtual PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size) = 0;

  // FlashFullscreen.
  virtual PP_Bool FlashIsFullscreen(PP_Instance instance) = 0;
  virtual PP_Bool FlashSetFullscreen(PP_Instance instance,
                                     PP_Bool fullscreen) = 0;
  virtual PP_Bool FlashGetScreenSize(PP_Instance instance, PP_Size* size) = 0;

  // Gamepad.
  virtual void SampleGamepads(PP_Instance instance,
                              PP_GamepadsData_Dev* data) = 0;

  // InputEvent.
  virtual int32_t RequestInputEvents(PP_Instance instance,
                                     uint32_t event_classes) = 0;
  virtual int32_t RequestFilteringInputEvents(PP_Instance instance,
                                              uint32_t event_classes) = 0;
  virtual void ClearInputEventRequest(PP_Instance instance,
                                      uint32_t event_classes) = 0;

  // Messaging.
  virtual void PostMessage(PP_Instance instance, PP_Var message) = 0;

  // MouseLock.
  virtual int32_t LockMouse(PP_Instance instance,
                            PP_CompletionCallback callback) = 0;
  virtual void UnlockMouse(PP_Instance instance) = 0;

  // Zoom.
  virtual void ZoomChanged(PP_Instance instance, double factor) = 0;
  virtual void ZoomLimitsChanged(PP_Instance instance,
                                 double minimum_factor,
                                 double maximium_factor) = 0;

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

  static const ApiID kApiID = API_ID_PPB_INSTANCE;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_INSTANCE_API_H_
