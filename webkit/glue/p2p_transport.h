// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_P2P_TRANSPORT_H_
#define WEBKIT_GLUE_P2P_TRANSPORT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "webkit/glue/webkit_glue_export.h"

namespace net {
class Socket;
}  // namespace net

namespace WebKit {
class WebFrame;
}  // namespace WebKit

namespace webkit_glue {

// Interface for P2P transport.
class P2PTransport {
 public:
  enum State {
    STATE_NONE = 0,
    STATE_WRITABLE = 1,
    STATE_READABLE = 2,
  };

  enum Protocol {
    PROTOCOL_UDP = 0,
    PROTOCOL_TCP = 1,
  };

  class EventHandler {
   public:
    virtual ~EventHandler() {}

    // Called for each local candidate.
    virtual void OnCandidateReady(const std::string& address) = 0;

    // Called when readable of writable state of the stream changes.
    virtual void OnStateChange(State state) = 0;

    // Called when an error occures (e.g. TCP handshake
    // failed). P2PTransport object is not usable after that and
    // should be destroyed.
    virtual void OnError(int error) = 0;
  };

  struct WEBKIT_GLUE_EXPORT Config {
    Config();
    ~Config();

    // STUN server address and port.
    std::string stun_server;
    int stun_server_port;

    // Relay server address and port.
    std::string relay_server;
    int relay_server_port;

    // Relay server username.
    std::string relay_username;

    // Relay server password.
    std::string relay_password;

    // When set to true relay is a legacy Google relay (not TURN
    // compliant).
    bool legacy_relay;

    // TCP window sizes. Default size is used when set to 0.
    int tcp_receive_window;
    int tcp_send_window;

    // Disables Neagle's algorithm when set to true.
    bool tcp_no_delay;

    // TCP ACK delay.
    int tcp_ack_delay_ms;

    // Disable TCP-based transport when set to true.
    bool disable_tcp_transport;
  };

  virtual ~P2PTransport() {}

  // Initialize transport using specified configuration. |web_frame|
  // is used to make HTTP requests to relay servers. Returns true
  // if initialization succeeded.
  virtual bool Init(WebKit::WebFrame* web_frame,
                    const std::string& name,
                    Protocol protocol,
                    const Config& config,
                    EventHandler* event_handler) = 0;

  // Add candidate received from the remote peer. Returns false if the
  // provided address is not in a valid format.
  virtual bool AddRemoteCandidate(const std::string& address) = 0;

  // Returns socket interface that can be used to send/receive
  // data. Returned object is owned by the transport. Pending calls on
  // the socket are canceled when the transport is destroyed.
  virtual net::Socket* GetChannel() = 0;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_P2P_TRANSPORT_H_
