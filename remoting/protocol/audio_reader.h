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

namespace net {
class StreamSocket;
}  // namespace net

namespace remoting {
namespace protocol {

class Session;
class SessionConfig;

class AudioReader {
 public:
  // The callback is called when initialization is finished. The
  // parameter is set to true on success.
  typedef base::Callback<void(bool)> InitializedCallback;

  virtual ~AudioReader();

  static scoped_ptr<AudioReader> Create(const SessionConfig& config);

  // Initializies the reader.
  void Init(Session* session,
            AudioStub* audio_stub,
            const InitializedCallback& callback);
  bool is_connected();

 private:
  explicit AudioReader(AudioPacket::Encoding encoding);

  void OnChannelReady(scoped_ptr<net::StreamSocket> socket);
  void OnNewData(scoped_ptr<AudioPacket> packet,
                 const base::Closure& done_task);

  Session* session_;

  InitializedCallback initialized_callback_;

  AudioPacket::Encoding encoding_;

  // TODO(sergeyu): Remove |channel_| and let |reader_| own it.
  scoped_ptr<net::StreamSocket> channel_;

  ProtobufMessageReader<AudioPacket> reader_;

  // The stub that processes all received packets.
  AudioStub* audio_stub_;

  DISALLOW_COPY_AND_ASSIGN(AudioReader);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUDIO_READER_H_
