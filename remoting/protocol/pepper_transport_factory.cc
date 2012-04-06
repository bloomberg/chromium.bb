// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pepper_transport_factory.h"

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
#include "remoting/protocol/pepper_transport_socket_adapter.h"
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

class PepperStreamTransport : public StreamTransport,
                              public PepperTransportSocketAdapter::Observer {
 public:
  PepperStreamTransport(pp::Instance* pp_instance);
  virtual ~PepperStreamTransport();

  // StreamTransport interface.
  virtual void Initialize(
      const std::string& name,
      const TransportConfig& config,
      Transport::EventHandler* event_handler,
      scoped_ptr<ChannelAuthenticator> authenticator) OVERRIDE;
  virtual void Connect(
      const StreamTransport::ConnectedCallback& callback) OVERRIDE;
  virtual void AddRemoteCandidate(const cricket::Candidate& candidate) OVERRIDE;
  virtual const std::string& name() const OVERRIDE;
  virtual bool is_connected() const OVERRIDE;

  // PepperTransportSocketAdapter::Observer interface.
  virtual void OnChannelDeleted() OVERRIDE;
  virtual void OnChannelNewLocalCandidate(
      const std::string& candidate) OVERRIDE;

 private:
  void OnP2PConnect(int result);
  void OnAuthenticationDone(net::Error error,
                            scoped_ptr<net::StreamSocket> socket);

  void NotifyConnected(scoped_ptr<net::StreamSocket> socket);
  void NotifyConnectFailed();

  pp::Instance* pp_instance_;
  std::string name_;
  TransportConfig config_;
  EventHandler* event_handler_;
  StreamTransport::ConnectedCallback callback_;
  scoped_ptr<ChannelAuthenticator> authenticator_;

  // We own |channel_| until it is connected. After that
  // |authenticator_| owns it.
  scoped_ptr<PepperTransportSocketAdapter> owned_channel_;
  PepperTransportSocketAdapter* channel_;

  // Indicates that we've finished connecting.
  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(PepperStreamTransport);
};

PepperStreamTransport::PepperStreamTransport(pp::Instance* pp_instance)
    : pp_instance_(pp_instance),
      event_handler_(NULL),
      channel_(NULL),
      connected_(false) {
}

PepperStreamTransport::~PepperStreamTransport() {
  DCHECK(event_handler_);
  event_handler_->OnTransportDeleted(this);
  // Channel should be already destroyed if we were connected.
  DCHECK(!connected_ || channel_ == NULL);
}

void PepperStreamTransport::Initialize(
    const std::string& name,
    const TransportConfig& config,
    Transport::EventHandler* event_handler,
    scoped_ptr<ChannelAuthenticator> authenticator) {
  DCHECK(CalledOnValidThread());

  DCHECK(!name.empty());
  DCHECK(event_handler);

  // Can be initialized only once.
  DCHECK(name_.empty());

  name_ = name;
  config_ = config;
  event_handler_ = event_handler;
  authenticator_ = authenticator.Pass();
}

void PepperStreamTransport::Connect(
    const StreamTransport::ConnectedCallback& callback) {
  DCHECK(CalledOnValidThread());

  // Initialize() must be called first.
  DCHECK(!name_.empty());

  callback_ = callback;

  pp::Transport_Dev* transport =
      new pp::Transport_Dev(pp_instance_, name_.c_str(),
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

  if (config_.nat_traversal_mode == TransportConfig::NAT_TRAVERSAL_ENABLED) {
    if (transport->SetProperty(
            PP_TRANSPORTPROPERTY_STUN_SERVER,
            pp::Var(config_.stun_server)) != PP_OK) {
      LOG(ERROR) << "Failed to set STUN server.";
    }

    if (transport->SetProperty(
            PP_TRANSPORTPROPERTY_RELAY_SERVER,
            pp::Var(config_.relay_server)) != PP_OK) {
      LOG(ERROR) << "Failed to set relay server.";
    }

    if (transport->SetProperty(
            PP_TRANSPORTPROPERTY_RELAY_USERNAME,
            pp::Var("1")) != PP_OK) {
      LOG(ERROR) << "Failed to set relay username.";
    }

    if (transport->SetProperty(
            PP_TRANSPORTPROPERTY_RELAY_PASSWORD,
            pp::Var(config_.relay_token)) != PP_OK) {
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

  int result = channel_->Connect(
      base::Bind(&PepperStreamTransport::OnP2PConnect, base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnP2PConnect(result);
}

void PepperStreamTransport::AddRemoteCandidate(
    const cricket::Candidate& candidate) {
  DCHECK(CalledOnValidThread());
  if (channel_)
    channel_->AddRemoteCandidate(jingle_glue::SerializeP2PCandidate(candidate));
}

const std::string& PepperStreamTransport::name() const {
  DCHECK(CalledOnValidThread());
  return name_;
}

bool PepperStreamTransport::is_connected() const {
  DCHECK(CalledOnValidThread());
  return connected_;
}

void PepperStreamTransport::OnChannelDeleted() {
  if (connected_) {
    channel_ = NULL;
    // The PepperTransportSocketAdapter is being deleted, so delete
    // the channel too.
    delete this;
  }
}

void PepperStreamTransport::OnChannelNewLocalCandidate(
    const std::string& candidate) {
  DCHECK(CalledOnValidThread());

  cricket::Candidate candidate_value;
  if (!jingle_glue::DeserializeP2PCandidate(candidate, &candidate_value)) {
    LOG(ERROR) << "Failed to parse candidate " << candidate;
  }
  event_handler_->OnTransportCandidate(this, candidate_value);
}

void PepperStreamTransport::OnP2PConnect(int result) {
  DCHECK(CalledOnValidThread());

  if (result != net::OK) {
    NotifyConnectFailed();
    return;
  }

  authenticator_->SecureAndAuthenticate(
      owned_channel_.PassAs<net::StreamSocket>(),
      base::Bind(&PepperStreamTransport::OnAuthenticationDone,
                 base::Unretained(this)));
}

void PepperStreamTransport::OnAuthenticationDone(
    net::Error error, scoped_ptr<net::StreamSocket> socket) {
  DCHECK(CalledOnValidThread());
  if (error != net::OK) {
    NotifyConnectFailed();
    return;
  }

  NotifyConnected(socket.Pass());
}

void PepperStreamTransport::NotifyConnected(
    scoped_ptr<net::StreamSocket> socket) {
  DCHECK(!connected_);
  connected_ = true;
  callback_.Run(socket.Pass());
}

void PepperStreamTransport::NotifyConnectFailed() {
  channel_ = NULL;
  owned_channel_.reset();
  authenticator_.reset();

  NotifyConnected(scoped_ptr<net::StreamSocket>(NULL));
}

}  // namespace

PepperTransportFactory::PepperTransportFactory(
    pp::Instance* pp_instance)
    : pp_instance_(pp_instance) {
}

PepperTransportFactory::~PepperTransportFactory() {
}

scoped_ptr<StreamTransport> PepperTransportFactory::CreateStreamTransport() {
  return scoped_ptr<StreamTransport>(new PepperStreamTransport(pp_instance_));
}

scoped_ptr<DatagramTransport>
PepperTransportFactory::CreateDatagramTransport() {
  NOTIMPLEMENTED();
  return scoped_ptr<DatagramTransport>(NULL);
}

}  // namespace protocol
}  // namespace remoting
