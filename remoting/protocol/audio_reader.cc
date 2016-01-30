// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/audio_reader.h"

#include "base/bind.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"

namespace remoting {
namespace protocol {

AudioReader::AudioReader(AudioStub* audio_stub)
    : ChannelDispatcherBase(kAudioChannelName),
      audio_stub_(audio_stub),
      parser_(base::Bind(&AudioReader::OnAudioPacket, base::Unretained(this)),
              reader()) {}

AudioReader::~AudioReader() {}

void AudioReader::OnAudioPacket(scoped_ptr<AudioPacket> audio_packet) {
  audio_stub_->ProcessAudioPacket(std::move(audio_packet),
                                  base::Bind(&base::DoNothing));
}

}  // namespace protocol
}  // namespace remoting
