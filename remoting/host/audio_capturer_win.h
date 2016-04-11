// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_CAPTURER_WIN_H_
#define REMOTING_HOST_AUDIO_CAPTURER_WIN_H_

#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_comptr.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/audio_silence_detector.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

class AudioCapturerWin : public AudioCapturer {
 public:
  AudioCapturerWin();
  ~AudioCapturerWin() override;

  // AudioCapturer interface.
  bool Start(const PacketCapturedCallback& callback) override;

 private:
  // Receives all packets from the audio capture endpoint buffer and pushes them
  // to the network.
  void DoCapture();

  // Returns current volume setting of the host, in range [0.0, 1.0]. If the
  // audio has been muted, this function returns 0. If Windows API returns error
  // (such as audio device has been disabled or unpluged), this function ignores
  // host volume setting, and returns 1.0.
  float GetAudioLevel();

  // Processes a series of samples, and executes callback if the packet is
  // qualified to be sent to client.
  void ProcessSamples(uint8_t* data, size_t frames);

  PacketCapturedCallback callback_;

  AudioPacket::SamplingRate sampling_rate_;

  std::unique_ptr<base::RepeatingTimer> capture_timer_;
  base::TimeDelta audio_device_period_;

  AudioSilenceDetector silence_detector_;

  base::win::ScopedCoMem<WAVEFORMATEX> wave_format_ex_;
  base::win::ScopedComPtr<IAudioCaptureClient> audio_capture_client_;
  base::win::ScopedComPtr<IAudioClient> audio_client_;
  base::win::ScopedComPtr<IMMDevice> mm_device_;
  base::win::ScopedComPtr<IAudioEndpointVolume> audio_volume_;

  HRESULT last_capture_error_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AudioCapturerWin);
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_CAPTURER_WIN_H_
