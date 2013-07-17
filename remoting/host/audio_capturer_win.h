// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_CAPTURER_WIN_H_
#define REMOTING_HOST_AUDIO_CAPTURER_WIN_H_

#include <audioclient.h>
#include <mmdeviceapi.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
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
  virtual ~AudioCapturerWin();

  // AudioCapturer interface.
  virtual bool Start(const PacketCapturedCallback& callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual bool IsStarted() OVERRIDE;

 private:
  // Receives all packets from the audio capture endpoint buffer and pushes them
  // to the network.
  void DoCapture();

  PacketCapturedCallback callback_;

  AudioPacket::SamplingRate sampling_rate_;

  scoped_ptr<base::RepeatingTimer<AudioCapturerWin> > capture_timer_;
  base::TimeDelta audio_device_period_;

  AudioSilenceDetector silence_detector_;

  base::win::ScopedCoMem<WAVEFORMATEX> wave_format_ex_;
  base::win::ScopedComPtr<IAudioCaptureClient> audio_capture_client_;
  base::win::ScopedComPtr<IAudioClient> audio_client_;
  base::win::ScopedComPtr<IMMDevice> mm_device_;

  HRESULT last_capture_error_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AudioCapturerWin);
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_CAPTURER_WIN_H_
