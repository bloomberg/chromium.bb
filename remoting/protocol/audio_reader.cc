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
    : session_(NULL),
      encoding_(encoding),
      audio_stub_(NULL) {
}

AudioReader::~AudioReader() {
  if (session_)
    session_->CancelChannelCreation(kAudioChannelName);
}

// static
scoped_ptr<AudioReader> AudioReader::Create(const SessionConfig& config) {
  if (!config.is_audio_enabled())
    return scoped_ptr<AudioReader>(NULL);
  // TODO(kxing): Support different session configurations.
  return scoped_ptr<AudioReader>(new AudioReader(AudioPacket::ENCODING_RAW));
}

void AudioReader::Init(protocol::Session* session,
                               AudioStub* audio_stub,
                               const InitializedCallback& callback) {
  session_ = session;
  initialized_callback_ = callback;
  audio_stub_ = audio_stub;

  session_->CreateStreamChannel(
      kAudioChannelName,
      base::Bind(&AudioReader::OnChannelReady, base::Unretained(this)));
}

bool AudioReader::is_connected() {
  return channel_.get() != NULL;
}

void AudioReader::OnChannelReady(scoped_ptr<net::StreamSocket> socket) {
  if (!socket.get()) {
    initialized_callback_.Run(false);
    return;
  }

  DCHECK(!channel_.get());
  channel_ = socket.Pass();
  reader_.Init(channel_.get(), base::Bind(&AudioReader::OnNewData,
                                          base::Unretained(this)));
  initialized_callback_.Run(true);
}

void AudioReader::OnNewData(scoped_ptr<AudioPacket> packet,
                                    const base::Closure& done_task) {
  audio_stub_->ProcessAudioPacket(packet.Pass(), done_task);
}

}  // namespace protocol
}  // namespace remoting
