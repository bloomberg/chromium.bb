// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_AUDIO_PLAYER_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_AUDIO_PLAYER_H_

#include "base/callback.h"
#include "ppapi/cpp/audio.h"
#include "ppapi/cpp/instance.h"
#include "remoting/client/audio_player.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

class PepperAudioPlayer : public AudioPlayer {
 public:
  explicit PepperAudioPlayer(pp::Instance* instance);
  ~PepperAudioPlayer() override;

  uint32 GetSamplesPerFrame() override;

  bool ResetAudioPlayer(AudioPacket::SamplingRate sampling_rate) override;

 private:
  pp::Instance* instance_;
  pp::Audio audio_;

  // The count of sample frames per channel in an audio buffer.
  uint32 samples_per_frame_;

  DISALLOW_COPY_AND_ASSIGN(PepperAudioPlayer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_AUDIO_PLAYER_H_
