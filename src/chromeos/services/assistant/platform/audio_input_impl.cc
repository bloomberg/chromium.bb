// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_impl.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chromeos/services/assistant/public/features.h"
#include "libassistant/shared/public/platform_audio_buffer.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "services/audio/public/cpp/device_factory.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace assistant {

namespace {

constexpr assistant_client::BufferFormat kFormat{
    16000 /* sample_rate */, assistant_client::INTERLEAVED_S32, 1 /* channels */
};

class DspHotwordStateManager : public AudioInputImpl::HotwordStateManager {
 public:
  DspHotwordStateManager(scoped_refptr<base::SequencedTaskRunner> task_runner,
                         AudioInputImpl* input)
      : task_runner_(task_runner), input_(input), weak_factory_(this) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
  }

  // HotwordStateManager overrides:
  // Runs on main thread.
  void OnConversationTurnStarted() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (second_phase_timer_.IsRunning()) {
      DCHECK(stream_state_ == StreamState::HOTWORD);
      second_phase_timer_.Stop();
      stream_state_ = StreamState::NORMAL;
    } else {
      // Handles user click on mic button.
      input_->RecreateAudioInputStream(false /* use_dsp */);
    }
  }

  // Runs on main thread.
  void OnConversationTurnFinished() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    input_->RecreateAudioInputStream(true /* use_dsp */);
    stream_state_ = StreamState::HOTWORD;
  }

  // Runs on audio service thread
  void OnCaptureDataArrived() override {
    // Posting to main thread to avoid timer's sequence check error.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DspHotwordStateManager::OnCaptureDataArrivedMainThread,
                       weak_factory_.GetWeakPtr()));
  }

  // Runs on main thread.
  void OnCaptureDataArrivedMainThread() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (stream_state_ == StreamState::HOTWORD &&
        !second_phase_timer_.IsRunning()) {
      // 1s from now, if OnConversationTurnStarted is not called, we assume that
      // libassistant has rejected the hotword supplied by DSP. Thus, we reset
      // and reopen the device on hotword state.
      second_phase_timer_.Start(
          FROM_HERE, base::TimeDelta::FromSeconds(1),
          base::BindRepeating(
              &DspHotwordStateManager::OnConversationTurnFinished,
              base::Unretained(this)));
    }
  }

 private:
  enum class StreamState {
    HOTWORD,
    NORMAL,
  };

  StreamState stream_state_ = StreamState::HOTWORD;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::OneShotTimer second_phase_timer_;
  AudioInputImpl* input_;
  base::WeakPtrFactory<DspHotwordStateManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DspHotwordStateManager);
};

class AudioInputBufferImpl : public assistant_client::AudioBuffer {
 public:
  AudioInputBufferImpl(const void* data, uint32_t frame_count)
      : data_(data), frame_count_(frame_count) {}
  ~AudioInputBufferImpl() override = default;

  // assistant_client::AudioBuffer overrides:
  assistant_client::BufferFormat GetFormat() const override { return kFormat; }
  const void* GetData() const override { return data_; }
  void* GetWritableData() override {
    NOTREACHED();
    return nullptr;
  }
  int GetFrameCount() const override { return frame_count_; }

 private:
  const void* data_;
  int frame_count_;
  DISALLOW_COPY_AND_ASSIGN(AudioInputBufferImpl);
};

}  // namespace

AudioInputImpl::AudioInputImpl(service_manager::Connector* connector)
    : connector_(connector),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DETACH_FROM_SEQUENCE(observer_sequence_checker_);

  if (features::IsDspHotwordEnabled()) {
    state_manager_ =
        std::make_unique<DspHotwordStateManager>(task_runner_, this);
  } else {
    state_manager_ = std::make_unique<HotwordStateManager>();
  }
}

AudioInputImpl::~AudioInputImpl() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  StopRecording();
}

// Runs on audio service thread.
void AudioInputImpl::Capture(const media::AudioBus* audio_source,
                             int audio_delay_milliseconds,
                             double volume,
                             bool key_pressed) {
  DCHECK_EQ(kFormat.num_channels, audio_source->channels());

  state_manager_->OnCaptureDataArrived();

  std::vector<int32_t> buffer(kFormat.num_channels * audio_source->frames());
  audio_source->ToInterleaved<media::SignedInt32SampleTypeTraits>(
      audio_source->frames(), buffer.data());
  int64_t time = base::TimeTicks::Now().since_origin().InMilliseconds() -
                 audio_delay_milliseconds;
  AudioInputBufferImpl input_buffer(buffer.data(), audio_source->frames());
  {
    base::AutoLock lock(lock_);
    for (auto* observer : observers_)
      observer->OnBufferAvailable(input_buffer, time);
  }

  captured_frames_count_ += audio_source->frames();
  if (VLOG_IS_ON(1)) {
    auto now = base::TimeTicks::Now();
    if ((now - last_frame_count_report_time_) >
        base::TimeDelta::FromMinutes(2)) {
      VLOG(1) << "Captured frames: " << captured_frames_count_;
      last_frame_count_report_time_ = now;
    }
  }
}

// Runs on audio service thread.
void AudioInputImpl::OnCaptureError(const std::string& message) {
  LOG(ERROR) << "Capture error " << message;
  base::AutoLock lock(lock_);
  for (auto* observer : observers_)
    observer->OnError(AudioInput::Error::FATAL_ERROR);
}

// Runs on audio service thread.
void AudioInputImpl::OnCaptureMuted(bool is_muted) {}

// Run on LibAssistant thread.
assistant_client::BufferFormat AudioInputImpl::GetFormat() const {
  return kFormat;
}

// Run on LibAssistant thread.
void AudioInputImpl::AddObserver(
    assistant_client::AudioInput::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer_sequence_checker_);
  VLOG(1) << "Add observer";
  bool have_first_observer = false;
  {
    base::AutoLock lock(lock_);
    observers_.push_back(observer);
    have_first_observer = observers_.size() == 1;
  }

  if (have_first_observer) {
    // Post to main thread runner to start audio recording. Assistant thread
    // does not have thread context defined in //base and will fail sequence
    // check in AudioCapturerSource::Start().
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioInputImpl::UpdateRecordingState,
                                          weak_factory_.GetWeakPtr()));
  }
}

// Run on LibAssistant thread.
void AudioInputImpl::RemoveObserver(
    assistant_client::AudioInput::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer_sequence_checker_);
  VLOG(1) << "Remove observer";
  bool have_no_observer = false;
  {
    base::AutoLock lock(lock_);
    base::Erase(observers_, observer);
    have_no_observer = observers_.size() == 0;
  }

  if (have_no_observer) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioInputImpl::UpdateRecordingState,
                                          weak_factory_.GetWeakPtr()));

    // Reset the sequence checker since assistant may call from different thread
    // after restart.
    DETACH_FROM_SEQUENCE(observer_sequence_checker_);
  }
}

void AudioInputImpl::SetMicState(bool mic_open) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (mic_open_ == mic_open)
    return;

  mic_open_ = mic_open;
  UpdateRecordingState();
}

void AudioInputImpl::OnConversationTurnStarted() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  state_manager_->OnConversationTurnStarted();
}

void AudioInputImpl::OnConversationTurnFinished() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  state_manager_->OnConversationTurnFinished();
}

void AudioInputImpl::OnHotwordEnabled(bool enable) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (default_on_ == enable)
    return;

  default_on_ = enable;
  UpdateRecordingState();
}

void AudioInputImpl::RecreateAudioInputStream(bool use_dsp) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  StopRecording();

  source_ = audio::CreateInputDevice(
      connector_->Clone(), media::AudioDeviceDescription::kDefaultDeviceId);
  // AUDIO_PCM_LINEAR and AUDIO_PCM_LOW_LATENCY are the same on CRAS.
  auto param = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, media::CHANNEL_LAYOUT_MONO,
      kFormat.sample_rate,
      kFormat.sample_rate / 10 /* buffer size for 100 ms */);

  if (use_dsp)
    param.set_effects(media::AudioParameters::PlatformEffectsMask::HOTWORD);

  source_->Initialize(param, this);
  source_->Start();
  VLOG(1) << "Start recording";
}

void AudioInputImpl::StartRecording() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!source_);
  RecreateAudioInputStream(features::IsDspHotwordEnabled());
}

void AudioInputImpl::StopRecording() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (source_) {
    VLOG(1) << "Stop recording";
    source_->Stop();
    source_.reset();
    VLOG(1) << "Ending captured frames: " << captured_frames_count_;
  }
}

void AudioInputImpl::UpdateRecordingState() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  bool should_start;
  {
    base::AutoLock lock(lock_);
    should_start = (default_on_ || mic_open_) && observers_.size() > 0;
  }

  if (!source_ && should_start)
    StartRecording();
  else if (source_ && !should_start)
    StopRecording();
}

}  // namespace assistant
}  // namespace chromeos
