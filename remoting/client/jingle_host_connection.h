// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// JingleHostConnection implements the HostConnection interface using
// libjingle as the transport protocol.
//
// Much of this class focuses on translating JingleClient and JingleChannel
// callbacks into HostConnection::HostEventCallback messages.
//
// The public API of this class is designed to be asynchronous, and thread
// safe for invocation from other threads.
//
// Internally though, all work delegeated to the |network_thread| given
// during construction.  Any event handlers running on the |network_thread|
// should not block.

#ifndef REMOTING_CLIENT_JINGLE_HOST_CONNECTION_H_
#define REMOTING_CLIENT_JINGLE_HOST_CONNECTION_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "remoting/base/protocol_decoder.h"
#include "remoting/client/client_context.h"
#include "remoting/client/host_connection.h"
#include "remoting/jingle_glue/jingle_channel.h"
#include "remoting/jingle_glue/jingle_client.h"

class MessageLoop;

namespace remoting {

class JingleThread;

struct ClientConfig;

class JingleHostConnection : public HostConnection,
                             public JingleChannel::Callback,
                             public JingleClient::Callback {
 public:
  explicit JingleHostConnection(ClientContext* context);
  virtual ~JingleHostConnection();

  virtual void Connect(const ClientConfig& config,
                       HostEventCallback* event_callback);
  virtual void Disconnect();

  // JingleChannel::Callback interface.
  virtual void OnStateChange(JingleChannel* channel,
                             JingleChannel::State state);
  virtual void OnPacketReceived(JingleChannel* channel,
                                scoped_refptr<media::DataBuffer> buffer);

  // JingleClient::Callback interface.
  virtual void OnStateChange(JingleClient* client, JingleClient::State state);
  virtual bool OnAcceptConnection(JingleClient* client, const std::string& jid,
                                  JingleChannel::Callback** callback);
  virtual void OnNewConnection(JingleClient* client,
                               scoped_refptr<JingleChannel> channel);

 private:
  MessageLoop* message_loop();

  void DoConnect(const ClientConfig& config,
                 HostEventCallback* event_callback);
  void DoDisconnect();

  ClientContext* context_;

  scoped_refptr<JingleClient> jingle_client_;
  scoped_refptr<JingleChannel> jingle_channel_;

  ProtocolDecoder decoder_;
  HostEventCallback* event_callback_;

  DISALLOW_COPY_AND_ASSIGN(JingleHostConnection);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::JingleHostConnection);

#endif  // REMOTING_CLIENT_JINGLE_HOST_CONNECTION_H_
