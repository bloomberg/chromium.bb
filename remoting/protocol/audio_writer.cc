// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/audio_writer.h"

#include "base/bind.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"

namespace remoting {
namespace protocol {

AudioWriter::AudioWriter()
    : ChannelDispatcherBase(kAudioChannelName) {
}

AudioWriter::~AudioWriter() {
}

void AudioWriter::OnInitialized() {
  // TODO(sergeyu): Provide a non-null WriteFailedCallback for the writer.
  buffered_writer_.Init(
      channel(), BufferedSocketWriter::WriteFailedCallback());
}

void AudioWriter::ProcessAudioPacket(scoped_ptr<AudioPacket> packet,
                                     const base::Closure& done) {
  buffered_writer_.Write(SerializeAndFrameMessage(*packet), done);
}

// static
scoped_ptr<AudioWriter> AudioWriter::Create(const SessionConfig& config) {
  if (!config.is_audio_enabled())
    return scoped_ptr<AudioWriter>();
  // TODO(kxing): Support different session configurations.
  return scoped_ptr<AudioWriter>(new AudioWriter());
}

}  // namespace protocol
}  // namespace remoting
