// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/audio_system_factory.h"

#include "media/audio/audio_system_impl.h"
#include "services/service_manager/public/cpp/connector.h"

namespace audio {

std::unique_ptr<media::AudioSystem> CreateAudioSystem(
    std::unique_ptr<service_manager::Connector> connector) {
  // TODO(olka): Switch to the service-based AudioSystem in the next CL.
  return media::AudioSystemImpl::CreateInstance();
}

}  // namespace audio
