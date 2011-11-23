// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pepper_stream_channel.h"

#include "base/bind.h"
#include "crypto/hmac.h"
#include "jingle/glue/utils.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/client_socket_factory.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/dev/transport_dev.h"
#include "ppapi/cpp/var.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/pepper_session.h"
#include "remoting/protocol/transport_config.h"
#include "third_party/libjingle/source/talk/p2p/base/candidate.h"

namespace remoting {
namespace protocol {

namespace {

// Value is choosen to balance the extra latency against the reduced
// load due to ACK traffic.
const int kTcpAckDelayMilliseconds = 10;

// Values for the TCP send and receive buffer size. This should be tuned to
// accomodate high latency network but not backlog the decoding pipeline.
const int kTcpReceiveBufferSize = 256 * 1024;
const int kTcpSendBufferSize = kTcpReceiveBufferSize + 30 * 1024;

}  // namespace

PepperStreamChannel::PepperStreamChannel(
    PepperSession* session,
    const std::string& name,
    const Session::StreamChannelCallback& callback)
    : session_(session),
      name_(name),
      callback_(callback),
      channel_(NULL),
      connected_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(p2p_connect_callback_(
          this, &PepperStreamChannel::OnP2PConnect)) {
}

PepperStreamChannel::~PepperStreamChannel() {
  session_->OnDeleteChannel(this);
  // Verify that the |channel_| is ether destroyed or we own it.
  DCHECK_EQ(channel_, owned_channel_.get());
  // Channel should be already destroyed if we were connected.
  DCHECK(!connected_ || channel_ == NULL);
}

void PepperStreamChannel::Connect(pp::Instance* pp_instance,
                                  const TransportConfig& transport_config,
                                  ChannelAuthenticator* authenticator) {
  DCHECK(CalledOnValidThread());

  authenticator_.reset(authenticator);

  pp::Transport_Dev* transport =
      new pp::Transport_Dev(pp_instance, name_.c_str(),
                            PP_TRANSPORTTYPE_STREAM);

  if (transport->SetProperty(PP_TRANSPORTPROPERTY_TCP_RECEIVE_WINDOW,
                             pp::Var(kTcpReceiveBufferSize)) != PP_OK) {
    LOG(ERROR) << "Failed to set TCP receive window";
  }
  if (transport->SetProperty(PP_TRANSPORTPROPERTY_TCP_SEND_WINDOW,
                             pp::Var(kTcpSendBufferSize)) != PP_OK) {
    LOG(ERROR) << "Failed to set TCP send window";
  }

  if (transport->SetProperty(PP_TRANSPORTPROPERTY_TCP_NO_DELAY,
                             pp::Var(true)) != PP_OK) {
    LOG(ERROR) << "Failed to set TCP_NODELAY";
  }

  if (transport->SetProperty(PP_TRANSPORTPROPERTY_TCP_ACK_DELAY,
                             pp::Var(kTcpAckDelayMilliseconds)) != PP_OK) {
    LOG(ERROR) << "Failed to set TCP ACK delay.";
  }

  if (transport_config.nat_traversal) {
    if (transport->SetProperty(
            PP_TRANSPORTPROPERTY_STUN_SERVER,
            pp::Var(transport_config.stun_server)) != PP_OK) {
      LOG(ERROR) << "Failed to set STUN server.";
    }

    if (transport->SetProperty(
            PP_TRANSPORTPROPERTY_RELAY_SERVER,
            pp::Var(transport_config.relay_server)) != PP_OK) {
      LOG(ERROR) << "Failed to set relay server.";
    }

    if (transport->SetProperty(
            PP_TRANSPORTPROPERTY_RELAY_PASSWORD,
            pp::Var(transport_config.relay_token)) != PP_OK) {
      LOG(ERROR) << "Failed to set relay token.";
    }

    if (transport->SetProperty(
            PP_TRANSPORTPROPERTY_RELAY_MODE,
            pp::Var(PP_TRANSPORTRELAYMODE_GOOGLE)) != PP_OK) {
      LOG(ERROR) << "Failed to set relay mode.";
    }
  }

  if (transport->SetProperty(PP_TRANSPORTPROPERTY_DISABLE_TCP_TRANSPORT,
                             pp::Var(true)) != PP_OK) {
    LOG(ERROR) << "Failed to set DISABLE_TCP_TRANSPORT flag.";
  }

  channel_ = new PepperTransportSocketAdapter(transport, name_, this);
  owned_channel_.reset(channel_);

  int result = channel_->Connect(&p2p_connect_callback_);
  if (result != net::ERR_IO_PENDING)
    OnP2PConnect(result);
}

void PepperStreamChannel::AddRemoveCandidate(
    const cricket::Candidate& candidate) {
  DCHECK(CalledOnValidThread());
  if (channel_)
    channel_->AddRemoteCandidate(jingle_glue::SerializeP2PCandidate(candidate));
}

const std::string& PepperStreamChannel::name() const {
  DCHECK(CalledOnValidThread());
  return name_;
}

bool PepperStreamChannel::is_connected() const {
  DCHECK(CalledOnValidThread());
  return connected_;
}

void PepperStreamChannel::OnChannelDeleted() {
  if (connected_) {
    channel_ = NULL;
    // The PepperTransportSocketAdapter is being deleted, so delete
    // the channel too.
    delete this;
  }
}

void PepperStreamChannel::OnChannelNewLocalCandidate(
    const std::string& candidate) {
  DCHECK(CalledOnValidThread());

  cricket::Candidate candidate_value;
  if (!jingle_glue::DeserializeP2PCandidate(candidate, &candidate_value)) {
    LOG(ERROR) << "Failed to parse candidate " << candidate;
  }
  session_->AddLocalCandidate(candidate_value);
}

void PepperStreamChannel::OnP2PConnect(int result) {
  DCHECK(CalledOnValidThread());

  if (result != net::OK)
    NotifyConnectFailed();

  authenticator_->SecureAndAuthenticate(owned_channel_.release(), base::Bind(
      &PepperStreamChannel::OnAuthenticationDone, base::Unretained(this)));
}


void PepperStreamChannel::OnAuthenticationDone(
    net::Error error, net::StreamSocket* socket) {
  DCHECK(CalledOnValidThread());
  if (error != net::OK) {
    NotifyConnectFailed();
    return;
  }

  NotifyConnected(socket);
}

void PepperStreamChannel::NotifyConnected(net::StreamSocket* socket) {
  DCHECK(!connected_);
  callback_.Run(socket);
  connected_ = true;
}

void PepperStreamChannel::NotifyConnectFailed() {
  channel_ = NULL;
  owned_channel_.reset();
  authenticator_.reset();

  NotifyConnected(NULL);
}

}  // namespace protocol
}  // namespace remoting
