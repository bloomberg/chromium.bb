// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_input_event_impl.h"

#include "ppapi/shared_impl/var.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using ppapi::InputEventData;
using ppapi::InputEventImpl;
using ppapi::StringVar;
using ppapi::thunk::PPB_InputEvent_API;

namespace webkit {
namespace ppapi {

PPB_InputEvent_Impl::PPB_InputEvent_Impl(PluginInstance* instance,
                                         const InputEventData& data)
    : Resource(instance),
      InputEventImpl(data) {
}

// static
PP_Resource PPB_InputEvent_Impl::Create(PluginInstance* instance,
                                        const InputEventData& data) {
  scoped_refptr<PPB_InputEvent_Impl> event(
      new PPB_InputEvent_Impl(instance, data));
  return event->GetReference();
}

PPB_InputEvent_API* PPB_InputEvent_Impl::AsPPB_InputEvent_API() {
  return this;
}

PP_Var PPB_InputEvent_Impl::StringToPPVar(const std::string& str) {
  return StringVar::StringToPPVar(instance()->module()->pp_module(), str);
}

}  // namespace ppapi
}  // namespace webkit
