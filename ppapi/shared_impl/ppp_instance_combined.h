// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPP_INSTANCE_COMBINED_H_
#define PPAPI_SHARED_IMPL_PPP_INSTANCE_COMBINED_H_

#include "base/basictypes.h"
#include "ppapi/c/ppp_instance.h"

// TODO(dmichael): This is here only for temporary backwards compatibility so
// that NaCl and other plugins aren't broken while the change propagates. This
// needs to be deleted in 14, because we don't intend to support PPP_Instance.
// HandleInputEvent.
// --- Begin backwards compatibility code.
union PP_InputEventData {
  struct PP_InputEvent_Key key;
  struct PP_InputEvent_Character character;
  struct PP_InputEvent_Mouse mouse;
  struct PP_InputEvent_Wheel wheel;
  char padding[64];
};
struct PP_InputEvent {
  PP_InputEvent_Type type;
  int32_t padding;
  PP_TimeTicks time_stamp;
  union PP_InputEventData u;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_InputEvent, 80);

#define PPP_INSTANCE_INTERFACE_0_5 "PPP_Instance;0.5"

struct PPP_Instance_0_5 {
  PP_Bool (*DidCreate)(PP_Instance instance,
                       uint32_t argc,
                       const char* argn[],
                       const char* argv[]);
  void (*DidDestroy)(PP_Instance instance);
  void (*DidChangeView)(PP_Instance instance,
                        const struct PP_Rect* position,
                        const struct PP_Rect* clip);
  void (*DidChangeFocus)(PP_Instance instance, PP_Bool has_focus);
  PP_Bool (*HandleInputEvent)(PP_Instance instance,
                              const struct PP_InputEvent* event);
  PP_Bool (*HandleDocumentLoad)(PP_Instance instance, PP_Resource url_loader);
};
// --- End backwards compatibility code.
namespace ppapi {

struct PPP_Instance_Combined : public PPP_Instance_1_0 {
 public:
  explicit PPP_Instance_Combined(const PPP_Instance_1_0& instance_if);
  explicit PPP_Instance_Combined(const PPP_Instance_0_5& instance_if);
  PP_Bool (*HandleInputEvent_0_5)(PP_Instance instance,
                                 const struct PP_InputEvent* event);

  DISALLOW_COPY_AND_ASSIGN(PPP_Instance_Combined);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPP_INSTANCE_COMBINED_H_

