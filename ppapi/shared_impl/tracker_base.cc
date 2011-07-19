// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/tracker_base.h"

#include "base/logging.h"

namespace ppapi {

static TrackerBase* (*g_global_getter)() = NULL;

// static
void TrackerBase::Init(TrackerBase* (*getter)()) {
  g_global_getter = getter;
}

// static
TrackerBase* TrackerBase::Get() {
  return g_global_getter();
}

}  // namespace ppapi
