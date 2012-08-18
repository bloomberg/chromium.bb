// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUDIO_READER_H_
#define REMOTING_PROTOCOL_AUDIO_READER_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/channel_dispatcher_base.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace remoting {
namespace protocol {

class Session;
class SessionConfig;

class AudioReader : public ChannelDispatcherBase {
 public:
  static scoped_ptr<AudioReader> Create(const SessionConfig& config);

  virtual ~AudioReader();

  void set_audio_stub(AudioStub* audio_stub) { audio_stub_ = audio_stub; }

 protected:
  virtual void OnInitialized() OVERRIDE;

 private:
  explicit AudioReader(AudioPacket::Encoding encoding);

  void OnNewData(scoped_ptr<AudioPacket> packet,
                 const base::Closure& done_task);

  AudioPacket::Encoding encoding_;

  ProtobufMessageReader<AudioPacket> reader_;

  // The stub that processes all received packets.
  AudioStub* audio_stub_;

  DISALLOW_COPY_AND_ASSIGN(AudioReader);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUDIO_READER_H_
