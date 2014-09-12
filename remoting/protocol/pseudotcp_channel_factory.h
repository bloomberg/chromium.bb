// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PSEUDOTCP_CHANNEL_FACTORY_H_
#define REMOTING_PROTOCOL_PSEUDOTCP_CHANNEL_FACTORY_H_

#include <map>

#include "base/basictypes.h"
#include "remoting/protocol/stream_channel_factory.h"

namespace remoting {
namespace protocol {

class DatagramChannelFactory;

// StreamChannelFactory that creates PseudoTCP-based stream channels that run on
// top of datagram channels created using specified |datagram_channel_factory|.
class PseudoTcpChannelFactory : public StreamChannelFactory {
 public:
  // |datagram_channel_factory| must outlive this object.
  explicit PseudoTcpChannelFactory(
      DatagramChannelFactory* datagram_channel_factory);
  virtual ~PseudoTcpChannelFactory();

  // StreamChannelFactory interface.
  virtual void CreateChannel(const std::string& name,
                             const ChannelCreatedCallback& callback) OVERRIDE;
  virtual void CancelChannelCreation(const std::string& name) OVERRIDE;

 private:
  typedef std::map<std::string, net::StreamSocket*> PendingSocketsMap;

  void OnDatagramChannelCreated(const std::string& name,
                                const ChannelCreatedCallback& callback,
                                scoped_ptr<net::Socket> socket);
  void OnPseudoTcpConnected(const std::string& name,
                            const ChannelCreatedCallback& callback,
                            int result);

  DatagramChannelFactory* datagram_channel_factory_;

  PendingSocketsMap pending_sockets_;

  DISALLOW_COPY_AND_ASSIGN(PseudoTcpChannelFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PSEUDOTCP_CHANNEL_FACTORY_H_
