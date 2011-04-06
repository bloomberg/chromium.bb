// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_P2P_TRANSPORT_H_
#define WEBKIT_GLUE_P2P_TRANSPORT_H_

#include <string>

namespace net {
class Socket;
}  // namespace net

namespace webkit_glue {

// Interface for P2P transport.
class P2PTransport {
 public:
  enum State {
    STATE_NONE = 0,
    STATE_WRITABLE = 1,
    STATE_READABLE = 2,
  };

  class EventHandler {
   public:
    virtual ~EventHandler() {}

    // Called for each local candidate.
    virtual void OnCandidateReady(const std::string& address) = 0;

    // Called when readable of writable state of the stream changes.
    virtual void OnStateChange(State state) = 0;
  };

  virtual ~P2PTransport() {}

  // Initialize transport using specified configuration. Returns true
  // if initialization succeeded.
  virtual bool Init(const std::string& name,
                    const std::string& config,
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
