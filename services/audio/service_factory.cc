// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/service_factory.h"

#include "base/time/time.h"
#include "services/audio/in_process_audio_manager_accessor.h"
#include "services/audio/service.h"
#include "services/service_manager/public/cpp/service.h"

namespace audio {

std::unique_ptr<service_manager::Service> CreateEmbeddedService(
    media::AudioManager* audio_manager) {
  return std::make_unique<Service>(
      std::make_unique<InProcessAudioManagerAccessor>(audio_manager),
      base::TimeDelta() /* do not quit if all clients disconnected */);
}

}  // namespace audio
