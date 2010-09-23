// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_private2.h"

#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/ppb_private2.h"

namespace pepper {

namespace {

void SetInstanceAlwaysOnTop(PP_Instance pp_instance, bool on_top) {
  PluginInstance* instance = PluginInstance::FromPPInstance(pp_instance);
  if (!instance)
    return;
  instance->set_always_on_top(on_top);
}

const PPB_Private2 ppb_private2 = {
  &SetInstanceAlwaysOnTop
};

}  // namespace

// static
const PPB_Private2* Private2::GetInterface() {
  return &ppb_private2;
}

}  // namespace pepper
