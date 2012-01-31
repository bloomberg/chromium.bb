// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_instance_shared.h"

#include <string>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

PPB_Instance_Shared::~PPB_Instance_Shared() {
}

void PPB_Instance_Shared::Log(PP_Instance instance,
                              PP_LogLevel_Dev level,
                              PP_Var value) {
  LogWithSource(instance, level, PP_MakeUndefined(), value);
}

void PPB_Instance_Shared::LogWithSource(PP_Instance instance,
                                        PP_LogLevel_Dev level,
                                        PP_Var source,
                                        PP_Var value) {
  // The source defaults to empty if it's not a string. The PpapiGlobals
  // implementation will convert the empty string to the module name if
  // possible.
  std::string source_str;
  if (source.type == PP_VARTYPE_STRING)
    source_str = Var::PPVarToLogString(source);
  std::string value_str = Var::PPVarToLogString(value);
  PpapiGlobals::Get()->LogWithSource(instance, level, source_str, value_str);
}

int32_t PPB_Instance_Shared::ValidateRequestInputEvents(
    bool is_filtering,
    uint32_t event_classes) {
  // See if any bits are set we don't know about.
  if (event_classes &
      ~static_cast<uint32_t>(PP_INPUTEVENT_CLASS_MOUSE |
                             PP_INPUTEVENT_CLASS_KEYBOARD |
                             PP_INPUTEVENT_CLASS_WHEEL |
                             PP_INPUTEVENT_CLASS_TOUCH |
                             PP_INPUTEVENT_CLASS_IME))
    return PP_ERROR_NOTSUPPORTED;

  // Everything else is valid.
  return PP_OK;
}

}  // namespace ppapi
