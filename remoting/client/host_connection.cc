// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/host_connection.h"

#include "remoting/base/constants.h"

namespace remoting {

HostConnection::HostConnection(ProtocolDecoder* decoder,
                               EventHandler* handler)
    : decoder_(decoder), handler_(handler) {
}

HostConnection::~HostConnection() {
  Disconnect();
}

void HostConnection::Connect(const std::string& username,
                             const std::string& auth_token,
                             const std::string& host_jid) {
  jingle_client_ = new JingleClient();
  jingle_client_->Init(username, auth_token, kChromotingTokenServiceName, this);
  jingle_channel_ = jingle_client_->Connect(host_jid, this);
}

void HostConnection::Disconnect() {
  if (jingle_channel_.get())
    jingle_channel_->Close();

  if (jingle_client_.get())
    jingle_client_->Close();
}

void HostConnection::OnStateChange(JingleChannel* channel,
                                   JingleChannel::State state) {
  DCHECK(handler_);
  if (state == JingleChannel::FAILED)
    handler_->OnConnectionFailed(this);
  else if (state == JingleChannel::CLOSED)
    handler_->OnConnectionClosed(this);
  else if (state == JingleChannel::OPEN)
    handler_->OnConnectionOpened(this);
}

void HostConnection::OnPacketReceived(JingleChannel* channel,
                                      scoped_refptr<media::DataBuffer> buffer) {
  HostMessageList list;
  decoder_->ParseHostMessages(buffer, &list);
  DCHECK(handler_);
  handler_->HandleMessages(this, &list);
}

// JingleClient::Callback interface.
void HostConnection::OnStateChange(JingleClient* client,
                                   JingleClient::State state) {
  DCHECK(client);
  DCHECK(handler_);
  if (state == JingleClient::CONNECTED) {
    LOG(INFO) << "Connected as: " << client->GetFullJid();
  } else if (state == JingleClient::CLOSED) {
    LOG(INFO) << "Connection closed.";
    handler_->OnConnectionClosed(this);
  }
}

bool HostConnection::OnAcceptConnection(JingleClient* client,
                                        const std::string& jid,
                                        JingleChannel::Callback** callback) {
  // Client rejects all connection.
  return false;
}

void HostConnection::OnNewConnection(JingleClient* client,
                                     scoped_refptr<JingleChannel> channel) {
  NOTREACHED() << "SimpleClient can't accept connection.";
}

}  // namespace remoting
