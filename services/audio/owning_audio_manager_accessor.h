// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_OWNING_AUDIO_MANAGER_ACCESSOR_H_
#define SERVICES_AUDIO_OWNING_AUDIO_MANAGER_ACCESSOR_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "services/audio/service.h"

namespace media {
class AudioLogFactory;
class AudioManager;
class AudioThread;
}  // namespace media

namespace audio {

// Lazily creates AudioManager using provided factory callback and controls its
// lifetime. Threading model of a created AudioManager is enforced: its main
// thread is the thread OwningAudioManagerAccessor lives on.
class OwningAudioManagerAccessor : public Service::AudioManagerAccessor {
 public:
  using AudioManagerFactoryCallback =
      base::OnceCallback<std::unique_ptr<media::AudioManager>(
          std::unique_ptr<media::AudioThread>,
          media::AudioLogFactory* audio_log_factory)>;

  explicit OwningAudioManagerAccessor(
      AudioManagerFactoryCallback audio_manager_factory_cb);
  ~OwningAudioManagerAccessor() override;

  media::AudioManager* GetAudioManager() final;
  void Shutdown() final;

 private:
  AudioManagerFactoryCallback audio_manager_factory_cb_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(OwningAudioManagerAccessor);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_OWNING_AUDIO_MANAGER_ACCESSOR_H_
