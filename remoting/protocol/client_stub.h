// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface of a client that receives commands from a Chromoting host.
//
// This interface is responsible for a subset of control messages sent to
// the Chromoting client.

#ifndef REMOTING_PROTOCOL_CLIENT_STUB_H_
#define REMOTING_PROTOCOL_CLIENT_STUB_H_

#include "base/basictypes.h"

class Task;

namespace remoting {
namespace protocol {

class LocalLoginStatus;
class NotifyResolutionRequest;

class ClientStub {
 public:
  ClientStub();
  virtual ~ClientStub();

  virtual void NotifyResolution(const NotifyResolutionRequest* msg,
                                Task* done) = 0;
  virtual void BeginSessionResponse(const LocalLoginStatus* msg,
                                    Task* done) = 0;

  // Called when the client has authenticated with the host to enable the
  // host->client control channel.
  // Before this is called, only a limited set of control messages will be
  // processed.
  void OnAuthenticated();

  // Has the client successfully authenticated with the host?
  // I.e., should we be processing control events?
  bool authenticated();

 private:
  // Initially false, this records whether the client has authenticated with
  // the host.
  bool authenticated_;

  DISALLOW_COPY_AND_ASSIGN(ClientStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_STUB_H_
