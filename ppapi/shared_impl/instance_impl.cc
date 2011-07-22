// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/instance_impl.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_input_event.h"

namespace ppapi {

InstanceImpl::~InstanceImpl() {
}

int32_t InstanceImpl::ValidateRequestInputEvents(bool is_filtering,
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
