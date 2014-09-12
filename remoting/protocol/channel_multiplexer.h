// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHANNEL_MULTIPLEXER_H_
#define REMOTING_PROTOCOL_CHANNEL_MULTIPLEXER_H_

#include "base/memory/weak_ptr.h"
#include "remoting/proto/mux.pb.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/stream_channel_factory.h"

namespace remoting {
namespace protocol {

class ChannelMultiplexer : public StreamChannelFactory {
 public:
  static const char kMuxChannelName[];

  // |factory| is used to create the channel upon which to multiplex.
  ChannelMultiplexer(StreamChannelFactory* factory,
                     const std::string& base_channel_name);
  virtual ~ChannelMultiplexer();

  // StreamChannelFactory interface.
  virtual void CreateChannel(const std::string& name,
                             const ChannelCreatedCallback& callback) OVERRIDE;
  virtual void CancelChannelCreation(const std::string& name) OVERRIDE;

 private:
  struct PendingChannel;
  class MuxChannel;
  class MuxSocket;
  friend class MuxChannel;

  // Callback for |base_channel_| creation.
  void OnBaseChannelReady(scoped_ptr<net::StreamSocket> socket);

  // Helper to create channels asynchronously.
  void DoCreatePendingChannels();

  // Helper method used to create channels.
  MuxChannel* GetOrCreateChannel(const std::string& name);

  // Error handling callback for |writer_|.
  void OnWriteFailed(int error);

  // Failed write notifier, queued asynchronously by OnWriteFailed().
  void NotifyWriteFailed(const std::string& name);

  // Callback for |reader_;
  void OnIncomingPacket(scoped_ptr<MultiplexPacket> packet,
                        const base::Closure& done_task);

  // Called by MuxChannel.
  bool DoWrite(scoped_ptr<MultiplexPacket> packet,
               const base::Closure& done_task);

  // Factory used to create |base_channel_|. Set to NULL once creation is
  // finished or failed.
  StreamChannelFactory* base_channel_factory_;

  // Name of the underlying channel.
  std::string base_channel_name_;

  // The channel over which to multiplex.
  scoped_ptr<net::StreamSocket> base_channel_;

  // List of requested channels while we are waiting for |base_channel_|.
  std::list<PendingChannel> pending_channels_;

  int next_channel_id_;
  std::map<std::string, MuxChannel*> channels_;

  // Channels are added to |channels_by_receive_id_| only after we receive
  // receive_id from the remote peer.
  std::map<int, MuxChannel*> channels_by_receive_id_;

  BufferedSocketWriter writer_;
  ProtobufMessageReader<MultiplexPacket> reader_;

  base::WeakPtrFactory<ChannelMultiplexer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChannelMultiplexer);
};

}  // namespace protocol
}  // namespace remoting


#endif  // REMOTING_PROTOCOL_CHANNEL_MULTIPLEXER_H_
