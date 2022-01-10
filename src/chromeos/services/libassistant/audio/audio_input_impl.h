// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_INPUT_IMPL_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_INPUT_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "media/base/audio_capturer_source.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace libassistant {

class AudioInputStream;
class AudioCapturer;

class AudioInputImpl : public assistant_client::AudioInput {
 public:
  explicit AudioInputImpl(const absl::optional<std::string>& device_id);
  AudioInputImpl(const AudioInputImpl&) = delete;
  AudioInputImpl& operator=(const AudioInputImpl&) = delete;
  ~AudioInputImpl() override;

  class HotwordStateManager {
   public:
    explicit HotwordStateManager(AudioInputImpl* audio_input_);
    HotwordStateManager(const HotwordStateManager&) = delete;
    HotwordStateManager& operator=(const HotwordStateManager&) = delete;
    virtual ~HotwordStateManager() = default;

    virtual void OnConversationTurnStarted() {}
    virtual void OnConversationTurnFinished() {}
    virtual void OnCaptureDataArrived() {}
    virtual void RecreateAudioInputStream();

   protected:
    AudioInputImpl* input_;
  };

  void Initialize(mojom::PlatformDelegate* platform_delegate);

  // assistant_client::AudioInput overrides. These function are called by
  // assistant from assistant thread, for which we should not assume any
  // //base related thread context to be in place.
  assistant_client::BufferFormat GetFormat() const override;
  void AddObserver(assistant_client::AudioInput::Observer* observer) override;
  void RemoveObserver(
      assistant_client::AudioInput::Observer* observer) override;

  // Called when the mic state associated with the interaction is changed.
  void SetMicState(bool mic_open);
  void OnConversationTurnStarted();
  void OnConversationTurnFinished();

  // Called when hotword enabled status changed.
  void OnHotwordEnabled(bool enable);

  void SetDeviceId(const absl::optional<std::string>& device_id);
  void SetHotwordDeviceId(const absl::optional<std::string>& device_id);

  // Called when the user opens/closes the lid.
  void OnLidStateChanged(mojom::LidState new_state);

  void RecreateAudioInputStream(bool use_dsp);

  bool IsHotwordAvailable() const;

  // Returns the recording state used in unittests.
  bool IsRecordingForTesting() const;
  // Returns if the hotword device is used for recording now.
  bool IsUsingHotwordDeviceForTesting() const;
  // Returns if the state of device's microphone is currently open.
  bool IsMicOpenForTesting() const;
  // Returns the id of the device that is currently recording audio.
  // Returns nullopt if no audio is being recorded.
  absl::optional<std::string> GetOpenDeviceIdForTesting() const;
  // Returns if dead stream detection is being used for the current audio
  // recording. Returns nullopt if no audio is being recorded.
  absl::optional<bool> IsUsingDeadStreamDetectionForTesting() const;

 private:
  void RecreateStateManager();
  void OnCaptureDataArrived();

  void StartRecording();
  void StopRecording();
  void UpdateRecordingState();

  std::string GetDeviceId(bool use_dsp) const;
  absl::optional<std::string> GetOpenDeviceId() const;
  bool ShouldEnableDeadStreamDetection(bool use_dsp) const;
  bool HasOpenAudioStream() const;

  // User explicitly requested to open microphone.
  bool mic_open_ = false;

  // Whether hotword is currently enabled.
  bool hotword_enabled_ = true;

  // To be initialized on assistant thread the first call to AddObserver.
  // It ensures that AddObserver / RemoveObserver are called on the same
  // sequence.
  SEQUENCE_CHECKER(observer_sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<HotwordStateManager> state_manager_;
  std::unique_ptr<AudioCapturer> audio_capturer_;

  // Owned by |LibassistantService|.
  mojom::PlatformDelegate* platform_delegate_ = nullptr;

  // Preferred audio input device which will be used for capture.
  absl::optional<std::string> preferred_device_id_;
  // Hotword input device used for hardware based hotword detection.
  absl::optional<std::string> hotword_device_id_;

  // Currently open audio stream. nullptr if no audio stream is open.
  std::unique_ptr<AudioInputStream> open_audio_stream_;

  // Start with lidstate |kClosed| so we do not open the microphone before we
  // know if the lid is open or closed.
  mojom::LidState lid_state_ = mojom::LidState ::kClosed;

  base::WeakPtrFactory<AudioInputImpl> weak_factory_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_INPUT_IMPL_H_
