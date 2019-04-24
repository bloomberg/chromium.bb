// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/timer/timer.h"
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

constexpr assistant_client::BufferFormat kFormatMono{
    16000 /* sample_rate */, assistant_client::INTERLEAVED_S32, 1 /* channels */
};

constexpr assistant_client::BufferFormat kFormatStereo{
    16000 /* sample_rate */, assistant_client::INTERLEAVED_S32, 2 /* channels */
};

assistant_client::BufferFormat g_current_format = kFormatMono;

media::ChannelLayout GetChannelLayout(
    const assistant_client::BufferFormat& format) {
  switch (format.num_channels) {
    case 1:
      return media::ChannelLayout::CHANNEL_LAYOUT_MONO;
    case 2:
      return media::ChannelLayout::CHANNEL_LAYOUT_STEREO;
    default:
      NOTREACHED();
      return media::ChannelLayout::CHANNEL_LAYOUT_UNSUPPORTED;
  }
}

class DspHotwordStateManager : public AudioInputImpl::HotwordStateManager {
 public:
  DspHotwordStateManager(scoped_refptr<base::SequencedTaskRunner> task_runner,
                         AudioInputImpl* input)
      : AudioInputImpl::HotwordStateManager(input),
        task_runner_(task_runner),
        weak_factory_(this) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
  }

  // HotwordStateManager overrides:
  // Runs on main thread.
  void OnConversationTurnStarted() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (second_phase_timer_.IsRunning()) {
      DCHECK(stream_state_ == StreamState::HOTWORD);
      second_phase_timer_.Stop();
    } else {
      // Handles user click on mic button.
      input_->RecreateAudioInputStream(false /* use_dsp */);
    }
    stream_state_ = StreamState::NORMAL;
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

  void RecreateAudioInputStream() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    input_->RecreateAudioInputStream(stream_state_ == StreamState::HOTWORD);
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
  base::WeakPtrFactory<DspHotwordStateManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DspHotwordStateManager);
};

class AudioInputBufferImpl : public assistant_client::AudioBuffer {
 public:
  AudioInputBufferImpl(const void* data, uint32_t frame_count)
      : data_(data), frame_count_(frame_count) {}
  ~AudioInputBufferImpl() override = default;

  // assistant_client::AudioBuffer overrides:
  assistant_client::BufferFormat GetFormat() const override {
    return g_current_format;
  }
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

AudioInputImpl::HotwordStateManager::HotwordStateManager(
    AudioInputImpl* audio_input)
    : input_(audio_input) {}

void AudioInputImpl::HotwordStateManager::RecreateAudioInputStream() {
  input_->RecreateAudioInputStream(/*use_dsp=*/false);
}

AudioInputImpl::AudioInputImpl(service_manager::Connector* connector,
                               const std::string& device_id,
                               const std::string& hotword_device_id)
    : connector_(connector),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      device_id_(device_id),
      hotword_device_id_(hotword_device_id),
      weak_factory_(this) {
  DETACH_FROM_SEQUENCE(observer_sequence_checker_);

  RecreateStateManager();
  if (features::IsStereoAudioInputEnabled())
    g_current_format = kFormatStereo;
  else
    g_current_format = kFormatMono;
}

AudioInputImpl::~AudioInputImpl() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  StopRecording();
}

void AudioInputImpl::RecreateStateManager() {
  if (IsHotwordAvailable()) {
    state_manager_ =
        std::make_unique<DspHotwordStateManager>(task_runner_, this);
  } else {
    state_manager_ = std::make_unique<HotwordStateManager>(this);
  }
}

// Runs on audio service thread.
void AudioInputImpl::Capture(const media::AudioBus* audio_source,
                             int audio_delay_milliseconds,
                             double volume,
                             bool key_pressed) {
  DCHECK_EQ(g_current_format.num_channels, audio_source->channels());

  state_manager_->OnCaptureDataArrived();

  std::vector<int32_t> buffer(audio_source->channels() *
                              audio_source->frames());
  audio_source->ToInterleaved<media::SignedInt32SampleTypeTraits>(
      audio_source->frames(), buffer.data());
  int64_t time = base::TimeTicks::Now().since_origin().InMilliseconds() -
                 audio_delay_milliseconds;
  AudioInputBufferImpl input_buffer(buffer.data(), audio_source->frames());
  {
    base::AutoLock lock(lock_);
    for (auto* observer : observers_)
      observer->OnAudioBufferAvailable(input_buffer, time);
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
    observer->OnAudioError(AudioInput::Error::FATAL_ERROR);
}

// Runs on audio service thread.
void AudioInputImpl::OnCaptureMuted(bool is_muted) {}

// Run on LibAssistant thread.
assistant_client::BufferFormat AudioInputImpl::GetFormat() const {
  return g_current_format;
}

// Run on LibAssistant thread.
void AudioInputImpl::AddObserver(
    assistant_client::AudioInput::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer_sequence_checker_);
  VLOG(1) << "Add observer";

  // Feed the observer one frame of empty data to work around crbug/942268
  std::vector<int32_t> buffer(g_current_format.num_channels);
  AudioInputBufferImpl input_buffer(buffer.data(), /*frame_count=*/1);
  observer->OnAudioBufferAvailable(input_buffer, /*timestamp=*/0);

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

void AudioInputImpl::SetDeviceId(const std::string& device_id) {
  if (device_id_ == device_id)
    return;

  device_id_ = device_id;
  if (source_)
    state_manager_->RecreateAudioInputStream();
}

void AudioInputImpl::SetHotwordDeviceId(const std::string& device_id) {
  if (hotword_device_id_ == device_id)
    return;

  hotword_device_id_ = device_id;
  RecreateStateManager();
  if (source_)
    state_manager_->RecreateAudioInputStream();
}

void AudioInputImpl::RecreateAudioInputStream(bool use_dsp) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  StopRecording();

  // AUDIO_PCM_LINEAR and AUDIO_PCM_LOW_LATENCY are the same on CRAS.
  auto param = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      GetChannelLayout(g_current_format), g_current_format.sample_rate,
      g_current_format.sample_rate / 10 /* buffer size for 100 ms */);

  std::string* device_id = &device_id_;
  if (use_dsp && !hotword_device_id_.empty()) {
    param.set_effects(media::AudioParameters::PlatformEffectsMask::HOTWORD);
    device_id = &hotword_device_id_;
  }
  source_ = audio::CreateInputDevice(connector_->Clone(), *device_id);

  source_->Initialize(param, this);
  source_->Start();
  VLOG(1) << "Start recording";
}

bool AudioInputImpl::IsHotwordAvailable() {
  return features::IsDspHotwordEnabled() && !hotword_device_id_.empty();
}

void AudioInputImpl::StartRecording() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!source_);
  RecreateAudioInputStream(IsHotwordAvailable());
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
