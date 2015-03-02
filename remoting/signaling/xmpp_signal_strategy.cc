// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/xmpp_signal_strategy.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "jingle/glue/chrome_async_socket.h"
#include "jingle/glue/task_pump.h"
#include "jingle/glue/xmpp_client_socket_factory.h"
#include "jingle/notifier/base/gaia_constants.h"
#include "jingle/notifier/base/gaia_token_pre_xmpp_auth.h"
#include "net/socket/client_socket_factory.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/webrtc/base/thread.h"
#include "third_party/webrtc/libjingle/xmpp/prexmppauth.h"
#include "third_party/webrtc/libjingle/xmpp/saslcookiemechanism.h"

const char kDefaultResourceName[] = "chromoting";

// Use 58 seconds keep-alive interval, in case routers terminate
// connections that are idle for more than a minute.
const int kKeepAliveIntervalSeconds = 50;

// Read buffer size used by ChromeAsyncSocket for read and write buffers.
//
// TODO(sergeyu): Currently jingle::ChromeAsyncSocket fails Write() when the
// write buffer is full and talk::XmppClient just ignores the error. As result
// chunks of data sent to the server are dropped (and they may not be full XMPP
// stanzas). The problem needs to be fixed either in XmppClient on
// ChromeAsyncSocket (e.g. ChromeAsyncSocket could close the connection when
// buffer is full).
const size_t kReadBufferSize = 64 * 1024;
const size_t kWriteBufferSize = 64 * 1024;

const int kDefaultXmppPort = 5222;
const int kDefaultHttpsPort = 443;

namespace remoting {

XmppSignalStrategy::XmppServerConfig::XmppServerConfig() {}
XmppSignalStrategy::XmppServerConfig::~XmppServerConfig() {}

XmppSignalStrategy::XmppSignalStrategy(
    net::ClientSocketFactory* socket_factory,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const XmppSignalStrategy::XmppServerConfig& xmpp_server_config)
    : socket_factory_(socket_factory),
      request_context_getter_(request_context_getter),
      resource_name_(kDefaultResourceName),
      xmpp_client_(nullptr),
      xmpp_server_config_(xmpp_server_config),
      state_(DISCONNECTED),
      error_(OK) {
#if defined(NDEBUG)
  CHECK(xmpp_server_config_.use_tls);
#endif
}

XmppSignalStrategy::~XmppSignalStrategy() {
  Disconnect();

  // Destroying task runner will destroy XmppClient, but XmppClient may be on
  // the stack and it doesn't handle this case properly, so we need to delay
  // destruction.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
      FROM_HERE, task_runner_.release());
}

void XmppSignalStrategy::Connect() {
  DCHECK(CalledOnValidThread());

  // Disconnect first if we are currently connected.
  Disconnect();

  buzz::XmppClientSettings settings;
  buzz::Jid login_jid(xmpp_server_config_.username);
  settings.set_user(login_jid.node());
  settings.set_host(login_jid.domain());
  settings.set_resource(resource_name_);
  settings.set_token_service("oauth2");
  settings.set_auth_token(buzz::AUTH_MECHANISM_GOOGLE_TOKEN,
                          xmpp_server_config_.auth_token);

  int port = xmpp_server_config_.port;

  // Port 5222 may be blocked by firewall. talk.google.com allows connections on
  // port 443 which can be used instead of 5222. The webapp still requests to
  // use port 5222 for backwards compatibility with older versions of the host
  // that do not pass correct |use_fake_ssl_client_socket| value to
  // XmppClientSocketFactory (and so cannot connect to port 443). In case we are
  // connecting to talk.google.com try to use port 443 anyway.
  //
  // TODO(sergeyu): Once all hosts support connections on port 443
  // the webapp needs to be updated to request port 443 and these 2 lines can be
  // removed. crbug.com/443384
  if (xmpp_server_config_.host == "talk.google.com" && port == kDefaultXmppPort)
    port = kDefaultHttpsPort;

  settings.set_server(
      rtc::SocketAddress(xmpp_server_config_.host, port));
  settings.set_use_tls(
      xmpp_server_config_.use_tls ? buzz::TLS_ENABLED : buzz::TLS_DISABLED);

  // Enable fake SSL handshake when connecting over HTTPS port.
  bool use_fake_ssl_client_socket = (port == kDefaultHttpsPort);

  scoped_ptr<jingle_glue::XmppClientSocketFactory> xmpp_socket_factory(
      new jingle_glue::XmppClientSocketFactory(socket_factory_,
                                               net::SSLConfig(),
                                               request_context_getter_,
                                               use_fake_ssl_client_socket));
  buzz::AsyncSocket* socket = new jingle_glue::ChromeAsyncSocket(
      xmpp_socket_factory.release(), kReadBufferSize, kWriteBufferSize);

  task_runner_.reset(new jingle_glue::TaskPump());
  xmpp_client_ = new buzz::XmppClient(task_runner_.get());
  xmpp_client_->Connect(
      settings, std::string(), socket, CreatePreXmppAuth(settings));
  xmpp_client_->SignalStateChange
      .connect(this, &XmppSignalStrategy::OnConnectionStateChanged);
  xmpp_client_->engine()->AddStanzaHandler(this, buzz::XmppEngine::HL_TYPE);
  xmpp_client_->Start();

  SetState(CONNECTING);
}

void XmppSignalStrategy::Disconnect() {
  DCHECK(CalledOnValidThread());

  if (xmpp_client_) {
    xmpp_client_->engine()->RemoveStanzaHandler(this);

    xmpp_client_->Disconnect();

    // |xmpp_client_| should be set to nullptr in OnConnectionStateChanged()
    // in response to Disconnect() call above.
    DCHECK(xmpp_client_ == nullptr);
  }
}

SignalStrategy::State XmppSignalStrategy::GetState() const {
  DCHECK(CalledOnValidThread());
  return state_;
}

SignalStrategy::Error XmppSignalStrategy::GetError() const {
  DCHECK(CalledOnValidThread());
  return error_;
}

std::string XmppSignalStrategy::GetLocalJid() const {
  DCHECK(CalledOnValidThread());
  return xmpp_client_->jid().Str();
}

void XmppSignalStrategy::AddListener(Listener* listener) {
  DCHECK(CalledOnValidThread());
  listeners_.AddObserver(listener);
}

void XmppSignalStrategy::RemoveListener(Listener* listener) {
  DCHECK(CalledOnValidThread());
  listeners_.RemoveObserver(listener);
}

bool XmppSignalStrategy::SendStanza(scoped_ptr<buzz::XmlElement> stanza) {
  DCHECK(CalledOnValidThread());
  if (!xmpp_client_) {
    VLOG(0) << "Dropping signalling message because XMPP "
        "connection has been terminated.";
    return false;
  }

  buzz::XmppReturnStatus status = xmpp_client_->SendStanza(stanza.release());
  return status == buzz::XMPP_RETURN_OK || status == buzz::XMPP_RETURN_PENDING;
}

std::string XmppSignalStrategy::GetNextId() {
  DCHECK(CalledOnValidThread());
  if (!xmpp_client_) {
    // If the connection has been terminated then it doesn't matter
    // what Id we return.
    return std::string();
  }
  return xmpp_client_->NextId();
}

bool XmppSignalStrategy::HandleStanza(const buzz::XmlElement* stanza) {
  DCHECK(CalledOnValidThread());
  ObserverListBase<Listener>::Iterator it(listeners_);
  Listener* listener;
  while ((listener = it.GetNext()) != nullptr) {
    if (listener->OnSignalStrategyIncomingStanza(stanza))
      return true;
  }
  return false;
}

void XmppSignalStrategy::SetAuthInfo(const std::string& username,
                                     const std::string& auth_token) {
  DCHECK(CalledOnValidThread());
  xmpp_server_config_.username = username;
  xmpp_server_config_.auth_token = auth_token;
}

void XmppSignalStrategy::SetResourceName(const std::string &resource_name) {
  DCHECK(CalledOnValidThread());
  resource_name_ = resource_name;
}

void XmppSignalStrategy::OnConnectionStateChanged(
    buzz::XmppEngine::State state) {
  DCHECK(CalledOnValidThread());

  if (state == buzz::XmppEngine::STATE_OPEN) {
    keep_alive_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kKeepAliveIntervalSeconds),
        this, &XmppSignalStrategy::SendKeepAlive);
    SetState(CONNECTED);
  } else if (state == buzz::XmppEngine::STATE_CLOSED) {
    // Make sure we dump errors to the log.
    int subcode;
    buzz::XmppEngine::Error error = xmpp_client_->GetError(&subcode);
    VLOG(0) << "XMPP connection was closed: error=" << error
            << ", subcode=" << subcode;

    keep_alive_timer_.Stop();

    // Client is destroyed by the TaskRunner after the client is
    // closed. Reset the pointer so we don't try to use it later.
    xmpp_client_ = nullptr;

    switch (error) {
      case buzz::XmppEngine::ERROR_UNAUTHORIZED:
      case buzz::XmppEngine::ERROR_AUTH:
      case buzz::XmppEngine::ERROR_MISSING_USERNAME:
        error_ = AUTHENTICATION_FAILED;
        break;

      default:
        error_ = NETWORK_ERROR;
    }

    SetState(DISCONNECTED);
  }
}

void XmppSignalStrategy::SetState(State new_state) {
  if (state_ != new_state) {
    state_ = new_state;
    FOR_EACH_OBSERVER(Listener, listeners_,
                      OnSignalStrategyStateChange(new_state));
  }
}

void XmppSignalStrategy::SendKeepAlive() {
  xmpp_client_->SendRaw(" ");
}

// static
buzz::PreXmppAuth* XmppSignalStrategy::CreatePreXmppAuth(
    const buzz::XmppClientSettings& settings) {
  buzz::Jid jid(settings.user(), settings.host(), buzz::STR_EMPTY);
  return new notifier::GaiaTokenPreXmppAuth(
      jid.Str(), settings.auth_token(), settings.token_service(), "X-OAUTH2");
}

}  // namespace remoting
