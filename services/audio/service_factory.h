// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SERVICE_FACTORY_H_
#define SERVICES_AUDIO_SERVICE_FACTORY_H_

#include <memory>

namespace service_manager {
class Service;
}

namespace media {
class AudioManager;
}  // namespace media

namespace audio {

// Create an instance of Audio service which will live in the current process
// on top of AudioManager instance belonging to that process. Must be called on
// the device thread of AudioManager.
std::unique_ptr<service_manager::Service> CreateEmbeddedService(
    media::AudioManager* audio_manager);

}  // namespace audio

#endif  // SERVICES_AUDIO_SERVICE_FACTORY_H_
