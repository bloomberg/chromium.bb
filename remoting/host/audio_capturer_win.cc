// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_capturer_win.h"

#include <avrt.h>
#include <mmreg.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"

namespace {
const int kChannels = 2;
const int kBytesPerSample = 2;
const int kBitsPerSample = kBytesPerSample * 8;
// Conversion factor from 100ns to 1ms.
const int k100nsPerMillisecond = 10000;

// Tolerance for catching packets of silence. If all samples have absolute
// value less than this threshold, the packet will be counted as a packet of
// silence. A value of 2 was chosen, because Windows can give samples of 1 and
// -1, even when no audio is playing.
const int kSilenceThreshold = 2;

// Lower bound for timer intervals, in milliseconds.
const int kMinTimerInterval = 30;

// Upper bound for the timer precision error, in milliseconds.
// Timers are supposed to be accurate to 20ms, so we use 30ms to be safe.
const int kMaxExpectedTimerLag = 30;
}  // namespace

namespace remoting {

class AudioCapturerWin::MMNotificationClient : public IMMNotificationClient {
 public:
  HRESULT __stdcall OnDefaultDeviceChanged(
      EDataFlow flow,
      ERole role,
      LPCWSTR pwstrDefaultDevice) override {
    base::AutoLock lock(lock_);
    default_audio_device_changed_ = true;
    return S_OK;
  }

  HRESULT __stdcall QueryInterface(REFIID iid, void** object) override {
    if (iid == IID_IUnknown || iid == __uuidof(IMMNotificationClient)) {
      *object = static_cast<IMMNotificationClient*>(this);
      return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
  }

  // No Ops overrides.
  HRESULT __stdcall OnDeviceAdded(LPCWSTR pwstrDeviceId) override {
    return S_OK;
  }
  HRESULT __stdcall OnDeviceRemoved(LPCWSTR pwstrDeviceId) override {
    return S_OK;
  }
  HRESULT __stdcall OnDeviceStateChanged(LPCWSTR pwstrDeviceId,
                                         DWORD dwNewState) override {
    return S_OK;
  }
  HRESULT __stdcall OnPropertyValueChanged(LPCWSTR pwstrDeviceId,
                                           const PROPERTYKEY key) override {
    return S_OK;
  }
  ULONG __stdcall AddRef() override { return 1; }
  ULONG __stdcall Release() override { return 1; }

  bool GetAndResetDefaultAudioDeviceChanged() {
    base::AutoLock lock(lock_);
    if (default_audio_device_changed_) {
      default_audio_device_changed_ = false;
      return true;
    }
    return false;
  }

 private:
  // |lock_| musted be locked when accessing |default_audio_device_changed_|.
  bool default_audio_device_changed_ = false;
  base::Lock lock_;
};

AudioCapturerWin::AudioCapturerWin()
    : sampling_rate_(AudioPacket::SAMPLING_RATE_INVALID),
      silence_detector_(kSilenceThreshold),
      mm_notification_client_(new MMNotificationClient()),
      last_capture_error_(S_OK) {
  thread_checker_.DetachFromThread();
}

AudioCapturerWin::~AudioCapturerWin() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_client_) {
    audio_client_->Stop();
  }
}

bool AudioCapturerWin::Start(const PacketCapturedCallback& callback) {
  callback_ = callback;

  if (!Initialize()) {
    return false;
  }

  // Initialize the capture timer and start capturing. Note, this timer won't
  // be reset or restarted in ResetAndInitialize() function. Which means we
  // expect the audio_device_period_ is a system wide configuration, it would
  // not be changed with the default audio device.
  capture_timer_.reset(new base::RepeatingTimer());
  capture_timer_->Start(FROM_HERE, audio_device_period_, this,
                        &AudioCapturerWin::DoCapture);
  return true;
}

bool AudioCapturerWin::ResetAndInitialize() {
  Deinitialize();
  if (!Initialize()) {
    Deinitialize();
    return false;
  }
  return true;
}

void AudioCapturerWin::Deinitialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  wave_format_ex_.Reset(nullptr);
  mm_device_enumerator_.Release();
  audio_capture_client_.Release();
  audio_client_.Release();
  mm_device_.Release();
  audio_volume_.Release();
}

bool AudioCapturerWin::Initialize() {
  DCHECK(!audio_capture_client_.get());
  DCHECK(!audio_client_.get());
  DCHECK(!mm_device_.get());
  DCHECK(!audio_volume_.get());
  DCHECK(static_cast<PWAVEFORMATEX>(wave_format_ex_) == nullptr);
  DCHECK(thread_checker_.CalledOnValidThread());

  HRESULT hr = S_OK;
  hr = mm_device_enumerator_.CreateInstance(__uuidof(MMDeviceEnumerator));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create IMMDeviceEnumerator. Error " << hr;
    return false;
  }

  hr = mm_device_enumerator_->RegisterEndpointNotificationCallback(
      mm_notification_client_.get());
  if (FAILED(hr)) {
    // We cannot predict which kind of error the API may return, but this is
    // not a fatal error.
    LOG(ERROR) << "Failed to register IMMNotificationClient. Error " << hr;
  }

  // Get the audio endpoint.
  hr = mm_device_enumerator_->GetDefaultAudioEndpoint(eRender, eConsole,
                                                      mm_device_.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get IMMDevice. Error " << hr;
    return false;
  }

  // Get an audio client.
  hr = mm_device_->Activate(__uuidof(IAudioClient),
                            CLSCTX_ALL,
                            nullptr,
                            audio_client_.ReceiveVoid());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get an IAudioClient. Error " << hr;
    return false;
  }

  REFERENCE_TIME device_period;
  hr = audio_client_->GetDevicePeriod(&device_period, nullptr);
  if (FAILED(hr)) {
    LOG(ERROR) << "IAudioClient::GetDevicePeriod failed. Error " << hr;
    return false;
  }
  // We round up, if |device_period| / |k100nsPerMillisecond|
  // is not a whole number.
  int device_period_in_milliseconds =
      1 + ((device_period - 1) / k100nsPerMillisecond);
  audio_device_period_ = base::TimeDelta::FromMilliseconds(
      std::max(device_period_in_milliseconds, kMinTimerInterval));

  // Get the wave format.
  hr = audio_client_->GetMixFormat(&wave_format_ex_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get WAVEFORMATEX. Error " << hr;
    return false;
  }

  // Set the wave format
  switch (wave_format_ex_->wFormatTag) {
    case WAVE_FORMAT_IEEE_FLOAT:
      // Intentional fall-through.
    case WAVE_FORMAT_PCM:
      if (!AudioCapturer::IsValidSampleRate(wave_format_ex_->nSamplesPerSec)) {
        LOG(ERROR) << "Host sampling rate is neither 44.1 kHz nor 48 kHz.";
        return false;
      }
      sampling_rate_ = static_cast<AudioPacket::SamplingRate>(
          wave_format_ex_->nSamplesPerSec);

      wave_format_ex_->wFormatTag = WAVE_FORMAT_PCM;
      wave_format_ex_->nChannels = kChannels;
      wave_format_ex_->wBitsPerSample = kBitsPerSample;
      wave_format_ex_->nBlockAlign = kChannels * kBytesPerSample;
      wave_format_ex_->nAvgBytesPerSec =
          sampling_rate_ * kChannels * kBytesPerSample;
      break;
    case WAVE_FORMAT_EXTENSIBLE: {
      PWAVEFORMATEXTENSIBLE wave_format_extensible =
          reinterpret_cast<WAVEFORMATEXTENSIBLE*>(
          static_cast<WAVEFORMATEX*>(wave_format_ex_));
      if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
                      wave_format_extensible->SubFormat)) {
        if (!AudioCapturer::IsValidSampleRate(
                wave_format_extensible->Format.nSamplesPerSec)) {
          LOG(ERROR) << "Host sampling rate is neither 44.1 kHz nor 48 kHz.";
          return false;
        }
        sampling_rate_ = static_cast<AudioPacket::SamplingRate>(
            wave_format_extensible->Format.nSamplesPerSec);

        wave_format_extensible->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
        wave_format_extensible->Samples.wValidBitsPerSample = kBitsPerSample;

        wave_format_extensible->Format.nChannels = kChannels;
        wave_format_extensible->Format.nSamplesPerSec = sampling_rate_;
        wave_format_extensible->Format.wBitsPerSample = kBitsPerSample;
        wave_format_extensible->Format.nBlockAlign =
            kChannels * kBytesPerSample;
        wave_format_extensible->Format.nAvgBytesPerSec =
            sampling_rate_ * kChannels * kBytesPerSample;
      } else {
        LOG(ERROR) << "Failed to force 16-bit samples";
        return false;
      }
      break;
    }
    default:
      LOG(ERROR) << "Failed to force 16-bit PCM";
      return false;
  }

  // Initialize the IAudioClient.
  hr = audio_client_->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      AUDCLNT_STREAMFLAGS_LOOPBACK,
      (kMaxExpectedTimerLag + audio_device_period_.InMilliseconds()) *
      k100nsPerMillisecond,
      0,
      wave_format_ex_,
      nullptr);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to initialize IAudioClient. Error " << hr;
    return false;
  }

  // Get an IAudioCaptureClient.
  hr = audio_client_->GetService(__uuidof(IAudioCaptureClient),
                                 audio_capture_client_.ReceiveVoid());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get an IAudioCaptureClient. Error " << hr;
    return false;
  }

  // Start the IAudioClient.
  hr = audio_client_->Start();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to start IAudioClient. Error " << hr;
    return false;
  }

  // Initialize IAudioEndpointVolume.
  // TODO(zijiehe): Do we need to control per process volume?
  hr = mm_device_->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                            audio_volume_.ReceiveVoid());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get an IAudioEndpointVolume. Error " << hr;
    return false;
  }

  silence_detector_.Reset(sampling_rate_, kChannels);

  return true;
}

bool AudioCapturerWin::is_initialized() const {
  // All Com components should be initialized / deinitialized together.
  return !!audio_volume_;
}

float AudioCapturerWin::GetAudioLevel() {
  BOOL mute;
  HRESULT hr = audio_volume_->GetMute(&mute);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get mute status from IAudioEndpointVolume, error "
               << hr;
    return 1;
  }
  if (mute) {
    return 0;
  }

  float level;
  hr = audio_volume_->GetMasterVolumeLevelScalar(&level);
  if (FAILED(hr) || level > 1) {
    LOG(ERROR) << "Failed to get master volume from IAudioEndpointVolume, "
                  "error "
               << hr;
    return 1;
  }
  if (level < 0) {
    return 0;
  }
  return level;
}

void AudioCapturerWin::ProcessSamples(uint8_t* data, size_t frames) {
  if (frames == 0) {
    return;
  }

  int16_t* samples = reinterpret_cast<int16_t*>(data);
  static_assert(sizeof(samples[0]) == kBytesPerSample,
                "expect 16 bits per sample");
  size_t sample_count = frames * kChannels;
  if (silence_detector_.IsSilence(samples, sample_count)) {
    return;
  }

  // Windows API does not provide volume adjusted audio sample as Linux does.
  // So we need to manually apply volume to the samples.
  float level = GetAudioLevel();
  if (level == 0) {
    return;
  }

  if (level < 1) {
    int32_t level_int = static_cast<int32_t>(level * 65536);
    for (size_t i = 0; i < sample_count; i++) {
      samples[i] = (static_cast<int32_t>(samples[i]) * level_int) >> 16;
    }
  }

  std::unique_ptr<AudioPacket> packet(new AudioPacket());
  packet->add_data(data, frames * wave_format_ex_->nBlockAlign);
  packet->set_encoding(AudioPacket::ENCODING_RAW);
  packet->set_sampling_rate(sampling_rate_);
  packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
  packet->set_channels(AudioPacket::CHANNELS_STEREO);

  callback_.Run(std::move(packet));
}

void AudioCapturerWin::DoCapture() {
  DCHECK(AudioCapturer::IsValidSampleRate(sampling_rate_));
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!is_initialized() ||
      mm_notification_client_->GetAndResetDefaultAudioDeviceChanged()) {
    if (!ResetAndInitialize()) {
      // Initialization failed, we should wait for next DoCapture call.
      return;
    }
  }

  // Fetch all packets from the audio capture endpoint buffer.
  HRESULT hr = S_OK;
  while (true) {
    UINT32 next_packet_size;
    HRESULT hr = audio_capture_client_->GetNextPacketSize(&next_packet_size);
    if (FAILED(hr))
      break;

    if (next_packet_size <= 0) {
      return;
    }

    BYTE* data;
    UINT32 frames;
    DWORD flags;
    hr = audio_capture_client_->GetBuffer(&data, &frames, &flags, nullptr,
                                          nullptr);
    if (FAILED(hr))
      break;

    ProcessSamples(data, frames);

    hr = audio_capture_client_->ReleaseBuffer(frames);
    if (FAILED(hr))
      break;
  }

  // There is nothing to capture if the audio endpoint device has been unplugged
  // or disabled.
  if (hr == AUDCLNT_E_DEVICE_INVALIDATED)
    return;

  // Avoid reporting the same error multiple times.
  if (FAILED(hr) && hr != last_capture_error_) {
    last_capture_error_ = hr;
    LOG(ERROR) << "Failed to capture an audio packet: 0x"
               << std::hex << hr << std::dec << ".";
  }
}

bool AudioCapturer::IsSupported() {
  return true;
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create() {
  return base::WrapUnique(new AudioCapturerWin());
}

}  // namespace remoting
