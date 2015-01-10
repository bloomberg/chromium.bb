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
      parser_(base::Bind(&AudioStub::ProcessAudioPacket,
                         base::Unretained(audio_stub)),
              reader()) {
}

AudioReader::~AudioReader() {
}

}  // namespace protocol
}  // namespace remoting
