// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_HOST_CONNECTION_H_
#define REMOTING_CLIENT_HOST_CONNECTION_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "remoting/base/protocol_decoder.h"

namespace remoting {

struct ClientConfig;

class HostConnection {
 public:
  class HostEventCallback {
   public:
    virtual ~HostEventCallback() {}

    // Handles an event received by the HostConnection. Receiver will own the
    // HostMessages in HostMessageList and needs to delete them.
    // Note that the sender of messages will not reference messages
    // again so it is okay to clear |messages| in this method.
    virtual void HandleMessages(HostConnection* conn,
                                HostMessageList* messages) = 0;

    // Called when the network connection is opened.
    virtual void OnConnectionOpened(HostConnection* conn) = 0;

    // Called when the network connection is closed.
    virtual void OnConnectionClosed(HostConnection* conn) = 0;

    // Called when the network connection has failed.
    virtual void OnConnectionFailed(HostConnection* conn) = 0;
  };

  virtual ~HostConnection() {}

  // TODO(ajwong): We need to generalize this API.
  virtual void Connect(const ClientConfig& config,
                       HostEventCallback* event_callback) = 0;
  virtual void Disconnect() = 0;

 protected:
  HostConnection() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HostConnection);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_HOST_CONNECTION_H_
