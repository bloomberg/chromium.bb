// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SERVICE_H_
#define SERVICES_AUDIO_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "services/audio/public/mojom/debug_recording.mojom.h"
#include "services/audio/public/mojom/system_info.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace media {
class AudioManager;
}  // namespace media

namespace service_manager {
class ServiceContextRefFactory;
}

namespace audio {
class DebugRecording;
class SystemInfo;

class Service : public service_manager::Service {
 public:
  // Abstracts AudioManager ownership. Lives and must be accessed on a thread
  // its created on, and that thread must be AudioManager main thread.
  class AudioManagerAccessor {
   public:
    virtual ~AudioManagerAccessor() {}

    // Must be called before destruction.
    virtual void Shutdown() = 0;

    // Returns a pointer to AudioManager.
    virtual media::AudioManager* GetAudioManager() = 0;
  };

  // Service will attempt to quit if there are no connections to it within
  // |quit_timeout| interval. If |quit_timeout| is base::TimeDelta() the
  // service never quits.
  Service(std::unique_ptr<AudioManagerAccessor> audio_manager_accessor,
          base::TimeDelta quit_timeout);
  ~Service() final;

  // service_manager::Service implementation.
  void OnStart() final;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) final;
  bool OnServiceManagerConnectionLost() final;

  void SetQuitClosureForTesting(base::RepeatingClosure quit_closure);

 private:
  void BindSystemInfoRequest(mojom::SystemInfoRequest request);
  void BindDebugRecordingRequest(mojom::DebugRecordingRequest request);

  void MaybeRequestQuitDelayed();
  void MaybeRequestQuit();

  // Thread it runs on should be the same as the main thread of AudioManager
  // provided by AudioManagerAccessor.
  THREAD_CHECKER(thread_checker_);

  // The members below should outlive |ref_factory_|.
  base::RepeatingClosure quit_closure_;
  const base::TimeDelta quit_timeout_;
  base::OneShotTimer quit_timer_;
  std::unique_ptr<service_manager::ServiceContextRefFactory> ref_factory_;

  std::unique_ptr<AudioManagerAccessor> audio_manager_accessor_;
  std::unique_ptr<SystemInfo> system_info_;
  std::unique_ptr<DebugRecording> debug_recording_;

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_SERVICE_H_
