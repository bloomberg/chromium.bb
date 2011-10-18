// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/host_globals.h"

namespace webkit {
namespace ppapi {

HostGlobals* HostGlobals::host_globals_ = NULL;

HostGlobals::HostGlobals() : ::ppapi::PpapiGlobals() {
  DCHECK(!host_globals_);
  host_globals_ = this;
}

HostGlobals::~HostGlobals() {
  DCHECK(host_globals_ == this);
  host_globals_ = NULL;
}

::ppapi::ResourceTracker* HostGlobals::GetResourceTracker() {
  return &host_resource_tracker_;
}

::ppapi::VarTracker* HostGlobals::GetVarTracker() {
  return &host_var_tracker_;
}

}  // namespace ppapi
}  // namespace webkit
