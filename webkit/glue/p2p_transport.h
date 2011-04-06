// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_P2P_TRANSPORT_H_
#define WEBKIT_GLUE_P2P_TRANSPORT_H_

#include <string>

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

    // Called when a message received from the peer. P2PTransport keeps
    // owneship of |data|.
    virtual void OnMessageReceived(const char* data, size_t data_size) = 0;
  };

  virtual ~P2PTransport() {}

  // Initialize transport using specified configuration.
  virtual void Init(const std::string& config,
                    EventHandler* event_handler) = 0;

  // Add candidate received from the remote peer.
  virtual void AddRemoteCandidate(const std::string& address) = 0;

  // Send data to the other end. Caller keeps ownership of |data|.
  virtual void Send(const char* data, int data_size) = 0;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_P2P_TRANSPORT_H_
