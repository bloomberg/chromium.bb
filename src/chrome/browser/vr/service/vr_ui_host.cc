// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/vr_ui_host.h"

namespace vr {

namespace {
VRUiHost::Factory* g_vr_ui_host_factory = nullptr;
}

// Needed for destructor chaining even for an abstract base class.
VRUiHost::~VRUiHost() = default;

// static
void VRUiHost::SetFactory(VRUiHost::Factory* factory) {
  DVLOG(1) << __func__;
  g_vr_ui_host_factory = factory;
}

// static
VRUiHost::Factory* VRUiHost::GetFactory() {
  DVLOG(1) << __func__;
  DCHECK(g_vr_ui_host_factory);
  return g_vr_ui_host_factory;
}

}  // namespace vr
