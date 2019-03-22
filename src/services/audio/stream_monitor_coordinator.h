// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_STREAM_MONITOR_COORDINATOR_H_
#define SERVICES_AUDIO_STREAM_MONITOR_COORDINATOR_H_

#include "services/audio/group_coordinator.h"
#include "services/audio/stream_monitor.h"

namespace audio {
using StreamMonitorCoordinator = GroupCoordinator<StreamMonitor>;
}  // namespace audio

#endif  // SERVICES_AUDIO_STREAM_MONITOR_COORDINATOR_H_
