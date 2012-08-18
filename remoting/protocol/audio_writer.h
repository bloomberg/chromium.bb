// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUDIO_WRITER_H_
#define REMOTING_PROTOCOL_AUDIO_WRITER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/protocol/channel_dispatcher_base.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace remoting {
namespace protocol {

class Session;
class SessionConfig;

class AudioWriter : public ChannelDispatcherBase,
                    public AudioStub {
 public:
  // Once AudioWriter is created, the Init() method of ChannelDispatcherBase
  // should be used to initialize it for the session.
  static scoped_ptr<AudioWriter> Create(const SessionConfig& config);

  virtual ~AudioWriter();

  // AudioStub interface.
  virtual void ProcessAudioPacket(scoped_ptr<AudioPacket> packet,
                                  const base::Closure& done) OVERRIDE;

 protected:
  virtual void OnInitialized() OVERRIDE;

 private:
  AudioWriter();

  BufferedSocketWriter buffered_writer_;

  DISALLOW_COPY_AND_ASSIGN(AudioWriter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUDIO_WRITER_H_
