// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_HOST_CONNECTION_H_
#define REMOTING_CLIENT_HOST_CONNECTION_H_

#include <deque>
#include <vector>

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "remoting/base/protocol_decoder.h"
#include "remoting/base/protocol/chromotocol.pb.h"
#include "remoting/jingle_glue/jingle_channel.h"
#include "remoting/jingle_glue/jingle_client.h"

namespace remoting {

class HostConnection : public JingleChannel::Callback,
                       public JingleClient::Callback {
 public:
  class EventHandler {
   public:
    virtual ~EventHandler() {}

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

  // Constructs a HostConnection object.
  HostConnection(ProtocolDecoder* decoder, EventHandler* handler);

  virtual ~HostConnection();

  void Connect(const std::string& username, const std::string& password,
               const std::string& host_jid);
  void Disconnect();

  // JingleChannel::Callback interface.
  void OnStateChange(JingleChannel* channel, JingleChannel::State state);
  void OnPacketReceived(JingleChannel* channel,
                        scoped_refptr<media::DataBuffer> buffer);

  // JingleClient::Callback interface.
  void OnStateChange(JingleClient* client, JingleClient::State state);
  bool OnAcceptConnection(JingleClient* client, const std::string& jid,
                          JingleChannel::Callback** callback);
  void OnNewConnection(JingleClient* client,
                       scoped_refptr<JingleChannel> channel);

 private:
  scoped_refptr<JingleClient> jingle_client_;
  scoped_refptr<JingleChannel> jingle_channel_;
  scoped_ptr<ProtocolDecoder> decoder_;
  EventHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(HostConnection);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_HOST_CONNECTION_H_
