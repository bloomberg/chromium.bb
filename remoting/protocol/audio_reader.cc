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

AudioReader::AudioReader(AudioPacket::Encoding encoding)
    : ChannelDispatcherBase(kAudioChannelName),
      encoding_(encoding),
      audio_stub_(NULL) {
}

AudioReader::~AudioReader() {
}

// static
scoped_ptr<AudioReader> AudioReader::Create(const SessionConfig& config) {
  if (!config.is_audio_enabled())
    return scoped_ptr<AudioReader>();
  // TODO(kxing): Support different session configurations.
  return scoped_ptr<AudioReader>(new AudioReader(AudioPacket::ENCODING_RAW));
}

void AudioReader::OnInitialized() {
  reader_.Init(channel(), base::Bind(&AudioReader::OnNewData,
                                     base::Unretained(this)));
}

void AudioReader::OnNewData(scoped_ptr<AudioPacket> packet,
                                    const base::Closure& done_task) {
  audio_stub_->ProcessAudioPacket(packet.Pass(), done_task);
}

}  // namespace protocol
}  // namespace remoting
