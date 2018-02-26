// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_DEBUG_RECORDING_SESSION_H_
#define SERVICES_AUDIO_PUBLIC_CPP_DEBUG_RECORDING_SESSION_H_

#include <memory>

#include "media/audio/audio_debug_recording_session.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/audio/public/mojom/debug_recording.mojom.h"

namespace service_manager {
class Connector;
}

namespace base {
class FilePath;
}

namespace audio {

class DebugRecordingFileProvider;

// Client class for using mojom::DebugRecording interface. This class owns
// mojom::DebugRecordingFileProvider implementation, therefore owners of this
// class' instances need permission to create files in |file_name_base| path
// passed in constructor in order to start debug recording. If file creation
// fails, debug recording will silently not start.
class DebugRecordingSession : public media::AudioDebugRecordingSession {
 public:
  class DebugRecordingFileProvider : public mojom::DebugRecordingFileProvider {
   public:
    DebugRecordingFileProvider(mojom::DebugRecordingFileProviderRequest request,
                               const base::FilePath& file_name_base);
    ~DebugRecordingFileProvider() override;

    // Creates file with name "|file_name_base_|.|file_suffix|.wav".
    void CreateWavFile(const base::FilePath& file_suffix,
                       CreateWavFileCallback reply_callback) override;

   private:
    mojo::Binding<mojom::DebugRecordingFileProvider> binding_;
    base::FilePath file_name_base_;

    DISALLOW_COPY_AND_ASSIGN(DebugRecordingFileProvider);
  };

  DebugRecordingSession(const base::FilePath& file_name_base,
                        std::unique_ptr<service_manager::Connector> connector);
  ~DebugRecordingSession() override;

 private:
  std::unique_ptr<DebugRecordingFileProvider> file_provider_;
  mojom::DebugRecordingPtr debug_recording_;

  DISALLOW_COPY_AND_ASSIGN(DebugRecordingSession);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_DEBUG_RECORDING_SESSION_H_
