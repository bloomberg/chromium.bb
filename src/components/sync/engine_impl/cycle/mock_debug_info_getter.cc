// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/cycle/mock_debug_info_getter.h"

namespace syncer {

MockDebugInfoGetter::MockDebugInfoGetter() {}

MockDebugInfoGetter::~MockDebugInfoGetter() {}

void MockDebugInfoGetter::GetDebugInfo(sync_pb::DebugInfo* debug_info) {
  debug_info->CopyFrom(debug_info_);
}

void MockDebugInfoGetter::ClearDebugInfo() {
  debug_info_.Clear();
}

void MockDebugInfoGetter::AddDebugEvent() {
  debug_info_.add_events();
}

}  // namespace syncer
