// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_capturer_win.h"

#include <windows.h>
#include <avrt.h>
#include <mmreg.h>
#include <mmsystem.h>

#include <algorithm>
#include <stdlib.h>

#include "base/logging.h"

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

AudioCapturerWin::AudioCapturerWin()
    : sampling_rate_(AudioPacket::SAMPLING_RATE_INVALID),
      silence_detector_(kSilenceThreshold),
      last_capture_error_(S_OK) {
    thread_checker_.DetachFromThread();
}

AudioCapturerWin::~AudioCapturerWin() {
}

bool AudioCapturerWin::Start(const PacketCapturedCallback& callback) {
  DCHECK(!audio_capture_client_.get());
  DCHECK(!audio_client_.get());
  DCHECK(!mm_device_.get());
  DCHECK(static_cast<PWAVEFORMATEX>(wave_format_ex_) == NULL);
  DCHECK(thread_checker_.CalledOnValidThread());

  callback_ = callback;

  // Initialize the capture timer.
  capture_timer_.reset(new base::RepeatingTimer<AudioCapturerWin>());

  HRESULT hr = S_OK;

  base::win::ScopedComPtr<IMMDeviceEnumerator> mm_device_enumerator;
  hr = mm_device_enumerator.CreateInstance(__uuidof(MMDeviceEnumerator));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create IMMDeviceEnumerator. Error " << hr;
    return false;
  }

  // Get the audio endpoint.
  hr = mm_device_enumerator->GetDefaultAudioEndpoint(eRender,
                                                     eConsole,
                                                     mm_device_.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get IMMDevice. Error " << hr;
    return false;
  }

  // Get an audio client.
  hr = mm_device_->Activate(__uuidof(IAudioClient),
                            CLSCTX_ALL,
                            NULL,
                            audio_client_.ReceiveVoid());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get an IAudioClient. Error " << hr;
    return false;
  }

  REFERENCE_TIME device_period;
  hr = audio_client_->GetDevicePeriod(&device_period, NULL);
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
      NULL);
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

  silence_detector_.Reset(sampling_rate_, kChannels);

  // Start capturing.
  capture_timer_->Start(FROM_HERE,
                        audio_device_period_,
                        this,
                        &AudioCapturerWin::DoCapture);
  return true;
}

void AudioCapturerWin::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsStarted());

  capture_timer_.reset();
  mm_device_.Release();
  audio_client_.Release();
  audio_capture_client_.Release();
  wave_format_ex_.Reset(NULL);

  thread_checker_.DetachFromThread();
}

bool AudioCapturerWin::IsStarted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return capture_timer_.get() != NULL;
}

void AudioCapturerWin::DoCapture() {
  DCHECK(AudioCapturer::IsValidSampleRate(sampling_rate_));
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsStarted());

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
    hr = audio_capture_client_->GetBuffer(&data, &frames, &flags, NULL, NULL);
    if (FAILED(hr))
      break;

    if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0 &&
        !silence_detector_.IsSilence(
            reinterpret_cast<const int16*>(data), frames * kChannels)) {
      scoped_ptr<AudioPacket> packet =
          scoped_ptr<AudioPacket>(new AudioPacket());
      packet->add_data(data, frames * wave_format_ex_->nBlockAlign);
      packet->set_encoding(AudioPacket::ENCODING_RAW);
      packet->set_sampling_rate(sampling_rate_);
      packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
      packet->set_channels(AudioPacket::CHANNELS_STEREO);

      callback_.Run(packet.Pass());
    }

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

scoped_ptr<AudioCapturer> AudioCapturer::Create() {
  return scoped_ptr<AudioCapturer>(new AudioCapturerWin());
}

}  // namespace remoting
