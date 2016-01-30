// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUDIO_READER_H_
#define REMOTING_PROTOCOL_AUDIO_READER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/protobuf_message_parser.h"

namespace remoting {
namespace protocol {

class AudioReader : public ChannelDispatcherBase {
 public:
  explicit AudioReader(AudioStub* audio_stub);
  ~AudioReader() override;

 private:
  void OnAudioPacket(scoped_ptr<AudioPacket> audio_packet);

  AudioStub* audio_stub_;
  ProtobufMessageParser<AudioPacket> parser_;

  DISALLOW_COPY_AND_ASSIGN(AudioReader);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUDIO_READER_H_
