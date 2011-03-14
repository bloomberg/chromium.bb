// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface of a host that receives commands from a Chromoting client.
//
// This interface handles control messages defined in contro.proto.

#ifndef REMOTING_PROTOCOL_HOST_STUB_H_
#define REMOTING_PROTOCOL_HOST_STUB_H_

#include "base/basictypes.h"

class Task;

namespace remoting {
namespace protocol {

class SuggestResolutionRequest;
class LocalLoginCredentials;

class HostStub {
 public:
  HostStub();
  virtual ~HostStub();

  virtual void SuggestResolution(
      const SuggestResolutionRequest* msg, Task* done) = 0;
  virtual void BeginSessionRequest(
      const LocalLoginCredentials* credentials, Task* done) = 0;

  // TODO(lambroslambrou): Remove OnAuthenticated() and OnClosed() when stubs
  // are refactored not to store authentication state.

  // Called when the client has authenticated with the host to enable the
  // client->host control channel.
  // Before this is called, only a limited set of control messages will be
  // processed.
  void OnAuthenticated();

  // Called when the client is no longer connected.
  void OnClosed();

  // Has the client successfully authenticated with the host?
  // I.e., should we be processing control events?
  bool authenticated();

 private:
  // Initially false, this records whether the client has authenticated with
  // the host.
  bool authenticated_;

  DISALLOW_COPY_AND_ASSIGN(HostStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_STUB_H_
