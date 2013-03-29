// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/extensions/dev/events_dev.h"

#include "ppapi/c/extensions/dev/ppb_ext_events_dev.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Ext_Events_Dev_0_1>() {
  return PPB_EXT_EVENTS_DEV_INTERFACE_0_1;
}

}  // namespace

namespace ext {
namespace events {

// static
uint32_t Events_Dev::AddListener(PP_Instance instance,
                                 const PP_Ext_EventListener& listener) {
  if (!has_interface<PPB_Ext_Events_Dev_0_1>())
    return 0;
  return get_interface<PPB_Ext_Events_Dev_0_1>()->AddListener(instance,
                                                              listener);
}

// static
void Events_Dev::RemoveListener(PP_Instance instance,
                                uint32_t listener_id) {
  if (has_interface<PPB_Ext_Events_Dev_0_1>()) {
    get_interface<PPB_Ext_Events_Dev_0_1>()->RemoveListener(instance,
                                                            listener_id);
  }
}

}  // namespace events
}  // namespace ext
}  // namespace pp
