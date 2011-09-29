// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/ime_input_event_dev.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_IMEInputEvent_Dev>() {
  return PPB_IME_INPUT_EVENT_DEV_INTERFACE;
}

}  // namespace

// IMEInputEvent_Dev -------------------------------------------------------

IMEInputEvent_Dev::IMEInputEvent_Dev() : InputEvent() {
}

IMEInputEvent_Dev::IMEInputEvent_Dev(const InputEvent& event) : InputEvent() {
  // Type check the input event before setting it.
  if (!has_interface<PPB_IMEInputEvent_Dev>())
    return;
  if (get_interface<PPB_IMEInputEvent_Dev>()->IsIMEInputEvent(
      event.pp_resource())) {
    Module::Get()->core()->AddRefResource(event.pp_resource());
    PassRefFromConstructor(event.pp_resource());
  }
}

Var IMEInputEvent_Dev::GetText() const {
  if (!has_interface<PPB_IMEInputEvent_Dev>())
    return Var();
  return Var(Var::PassRef(),
             get_interface<PPB_IMEInputEvent_Dev>()->GetText(pp_resource()));
}

uint32_t IMEInputEvent_Dev::GetSegmentNumber() const {
  if (!has_interface<PPB_IMEInputEvent_Dev>())
    return 0;
  return get_interface<PPB_IMEInputEvent_Dev>()->GetSegmentNumber(
      pp_resource());
}

uint32_t IMEInputEvent_Dev::GetSegmentOffset(uint32_t index) const {
  if (!has_interface<PPB_IMEInputEvent_Dev>())
    return 0;
  return get_interface<PPB_IMEInputEvent_Dev>()->GetSegmentOffset(pp_resource(),
                                                                  index);
}

int32_t IMEInputEvent_Dev::GetTargetSegment() const {
  if (!has_interface<PPB_IMEInputEvent_Dev>())
    return 0;
  return get_interface<PPB_IMEInputEvent_Dev>()->GetTargetSegment(
      pp_resource());
}

std::pair<uint32_t, uint32_t> IMEInputEvent_Dev::GetSelection() const {
  std::pair<uint32_t, uint32_t> range(0, 0);
  if (!has_interface<PPB_IMEInputEvent_Dev>())
    return range;
  get_interface<PPB_IMEInputEvent_Dev>()->GetSelection(pp_resource(),
                                                       &range.first,
                                                       &range.second);
  return range;
}


}  // namespace pp
