// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/audio_writer.h"

#include "base/bind.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

AudioWriter::AudioWriter()
    : session_(NULL) {
}

AudioWriter::~AudioWriter() {
  Close();
}

void AudioWriter::Init(protocol::Session* session,
                               const InitializedCallback& callback) {
  session_ = session;
  initialized_callback_ = callback;

  session_->CreateStreamChannel(
      kAudioChannelName,
      base::Bind(&AudioWriter::OnChannelReady, base::Unretained(this)));
}

void AudioWriter::OnChannelReady(scoped_ptr<net::StreamSocket> socket) {
  if (!socket.get()) {
    initialized_callback_.Run(false);
    return;
  }

  DCHECK(!channel_.get());
  channel_ = socket.Pass();
  // TODO(sergeyu): Provide WriteFailedCallback for the buffered writer.
  buffered_writer_.Init(
      channel_.get(), BufferedSocketWriter::WriteFailedCallback());

  initialized_callback_.Run(true);
}

void AudioWriter::Close() {
  buffered_writer_.Close();
  channel_.reset();
  if (session_) {
    session_->CancelChannelCreation(kAudioChannelName);
    session_ = NULL;
  }
}

bool AudioWriter::is_connected() {
  return channel_.get() != NULL;
}

void AudioWriter::ProcessAudioPacket(scoped_ptr<AudioPacket> packet,
                                             const base::Closure& done) {
  buffered_writer_.Write(SerializeAndFrameMessage(*packet), done);
}

// static
scoped_ptr<AudioWriter> AudioWriter::Create(const SessionConfig& config) {
  if (!config.is_audio_enabled())
    return scoped_ptr<AudioWriter>(NULL);
  // TODO(kxing): Support different session configurations.
  return scoped_ptr<AudioWriter>(new AudioWriter());
}

}  // namespace protocol
}  // namespace remoting
