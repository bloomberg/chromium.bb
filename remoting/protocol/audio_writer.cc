// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/audio_writer.h"

#include "base/bind.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/base/constants.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"

namespace remoting {
namespace protocol {

AudioWriter::AudioWriter() : ChannelDispatcherBase(kAudioChannelName) {}
AudioWriter::~AudioWriter() {}

void AudioWriter::ProcessAudioPacket(scoped_ptr<AudioPacket> packet,
                                     const base::Closure& done) {
  message_pipe()->Send(packet.get(), done);
}

// static
scoped_ptr<AudioWriter> AudioWriter::Create(const SessionConfig& config) {
  if (!config.is_audio_enabled())
    return nullptr;
  return make_scoped_ptr(new AudioWriter());
}

void AudioWriter::OnIncomingMessage(scoped_ptr<CompoundBuffer> message) {
  LOG(ERROR) << "Received unexpected message on the audio channel.";
}

}  // namespace protocol
}  // namespace remoting
