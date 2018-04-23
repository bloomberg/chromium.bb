// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worker_animation_frame_provider.h"

namespace blink {

WorkerAnimationFrameProvider::WorkerAnimationFrameProvider(
    const BeginFrameProviderParams& begin_frame_provider_params)
    : begin_frame_provider_(
          std::make_unique<BeginFrameProvider>(begin_frame_provider_params)) {}

}  // namespace blink
