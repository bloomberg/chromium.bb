// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_TRANSPORT_H_
#define REMOTING_PROTOCOL_TRANSPORT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/ip_endpoint.h"
#include "remoting/protocol/errors.h"

namespace cricket {
class Candidate;
}  // namespace cricket

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {
namespace protocol {

class Authenticator;
class DatagramChannelFactory;
class P2PDatagramSocket;
class StreamChannelFactory;

enum class TransportRole {
  SERVER,
  CLIENT,
};

struct TransportRoute {
  enum RouteType {
    DIRECT,
    STUN,
    RELAY,
  };

  // Helper method to get string representation of the type.
  static std::string GetTypeString(RouteType type);

  TransportRoute();
  ~TransportRoute();

  RouteType type;
  net::IPEndPoint remote_address;
  net::IPEndPoint local_address;
};

// Transport represents a P2P connection that consists of one or more
// channels.
class Transport {
 public:
  class EventHandler {
   public:
    // Called to send a transport-info message.
    virtual void OnOutgoingTransportInfo(
        scoped_ptr<buzz::XmlElement> message) = 0;

    // Called when transport route changes.
    virtual void OnTransportRouteChange(const std::string& channel_name,
                                        const TransportRoute& route) = 0;

    // Called when there is an error connecting the session.
    virtual void OnTransportError(ErrorCode error) = 0;
  };

  Transport() {}
  virtual ~Transport() {}

  // Starts transport session. Both parameters must outlive Transport.
  virtual void Start(EventHandler* event_handler,
                     Authenticator* authenticator) = 0;

  // Called to process incoming transport message. Returns false if
  // |transport_info| is in invalid format.
  virtual bool ProcessTransportInfo(buzz::XmlElement* transport_info) = 0;

  // Channel factory for the session that creates raw ICE channels.
  virtual DatagramChannelFactory* GetDatagramChannelFactory() = 0;

  // Channel factory for the session that creates stream channels.
  virtual StreamChannelFactory* GetStreamChannelFactory() = 0;

  // Returns a factory that creates multiplexed channels over a single stream
  // channel.
  virtual StreamChannelFactory* GetMultiplexedChannelFactory() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Transport);
};

class TransportFactory {
 public:
  TransportFactory() { }
  virtual ~TransportFactory() { }

  // Creates a new Transport. The factory must outlive the session.
  virtual scoped_ptr<Transport> CreateTransport() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TransportFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_TRANSPORT_H_
