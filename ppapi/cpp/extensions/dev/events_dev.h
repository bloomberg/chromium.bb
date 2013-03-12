// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_EXTENSIONS_DEV_EVENTS_DEV_H_
#define PPAPI_CPP_EXTENSIONS_DEV_EVENTS_DEV_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"

struct PP_Ext_EventListener;

namespace pp {
namespace ext {
namespace events {

// This is a simple wrapper of the PPB_Ext_Events_Dev interface.
//
// Usually you don't have to directly use this interface. Instead, you could
// use those more object-oriented event classes. Please see
// ppapi/cpp/extensions/event_base.h for more details.
class Events_Dev {
 public:
  static uint32_t AddListener(PP_Instance instance,
                              const PP_Ext_EventListener& listener);

  static void RemoveListener(PP_Instance instance, uint32_t listener_id);
};

}  // namespace events
}  // namespace ext
}  // namespace pp

#endif  // PPAPI_CPP_EXTENSIONS_DEV_EVENTS_DEV_H_
