// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUDIO_WRITER_H_
#define REMOTING_PROTOCOL_AUDIO_WRITER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/channel_dispatcher_base.h"

namespace remoting {
namespace protocol {

class SessionConfig;

class AudioWriter : public ChannelDispatcherBase,
                    public AudioStub {
 public:
  // Once AudioWriter is created, the Init() method of ChannelDispatcherBase
  // should be used to initialize it for the session.
  static std::unique_ptr<AudioWriter> Create(const SessionConfig& config);

  ~AudioWriter() override;

  // AudioStub interface.
  void ProcessAudioPacket(std::unique_ptr<AudioPacket> packet,
                          base::OnceClosure done) override;

 private:
  AudioWriter();

  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;

  DISALLOW_COPY_AND_ASSIGN(AudioWriter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUDIO_WRITER_H_
