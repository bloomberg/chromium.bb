// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/stream_monitor_coordinator.h"
#include "services/audio/group_coordinator-impl.h"

namespace audio {
template class GroupCoordinator<StreamMonitor>;
}  // namespace audio
