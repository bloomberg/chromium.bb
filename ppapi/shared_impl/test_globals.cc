// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/test_globals.h"

namespace ppapi {

TestGlobals::TestGlobals() : ppapi::PpapiGlobals() {
}

TestGlobals::~TestGlobals() {
}

ResourceTracker* TestGlobals::GetResourceTracker() {
  return &resource_tracker_;
}

VarTracker* TestGlobals::GetVarTracker() {
  return &var_tracker_;
}

}  // namespace ppapi
