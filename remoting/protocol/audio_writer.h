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

namespace net {
class StreamSocket;
}  // namespace net

namespace remoting {
namespace protocol {

class Session;
class SessionConfig;

class AudioWriter : public AudioStub {
 public:
  virtual ~AudioWriter();

  // The callback is called when initialization is finished. The
  // parameter is set to true on success.
  typedef base::Callback<void(bool)> InitializedCallback;

  static AudioWriter* Create(const SessionConfig& config);

  // Initializes the writer.
  void Init(Session* session, const InitializedCallback& callback);

  // Stops writing. Must be called on the network thread before this
  // object is destroyed.
  void Close();

  // Returns true if the channel is connected.
  bool is_connected();

  // AudioStub interface.
  virtual void ProcessAudioPacket(scoped_ptr<AudioPacket> packet,
                                  const base::Closure& done) OVERRIDE;

 private:
  AudioWriter();

  void OnChannelReady(scoped_ptr<net::StreamSocket> socket);

  Session* session_;

  InitializedCallback initialized_callback_;

  // TODO(sergeyu): Remove |channel_| and let |buffered_writer_| own it.
  scoped_ptr<net::StreamSocket> channel_;

  BufferedSocketWriter buffered_writer_;

  DISALLOW_COPY_AND_ASSIGN(AudioWriter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUDIO_WRITER_H_
