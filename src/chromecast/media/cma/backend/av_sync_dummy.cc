// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/av_sync.h"

#include "base/single_thread_task_runner.h"

namespace chromecast {
namespace media {

std::unique_ptr<AvSync> AvSync::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaPipelineBackendForMixer* const backend) {
  return nullptr;
}

}  // namespace media
}  // namespace chromecast
