// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/xmpp_signal_strategy.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "jingle/notifier/base/gaia_token_pre_xmpp_auth.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/xmpp_socket_adapter.h"
#include "third_party/libjingle/source/talk/base/asyncsocket.h"
#include "third_party/libjingle/source/talk/xmpp/prexmppauth.h"
#include "third_party/libjingle/source/talk/xmpp/saslcookiemechanism.h"

namespace {

const char kDefaultResourceName[] = "chromoting";

// Use 58 seconds keep-alive interval, in case routers terminate
// connections that are idle for more than a minute.
const int kKeepAliveIntervalSeconds = 50;

void DisconnectXmppClient(buzz::XmppClient* client) {
  client->Disconnect();
}

}  // namespace

namespace remoting {

XmppSignalStrategy::XmppSignalStrategy(JingleThread* jingle_thread,
                                       const std::string& username,
                                       const std::string& auth_token,
                                       const std::string& auth_token_service)
   : thread_(jingle_thread),
     username_(username),
     auth_token_(auth_token),
     auth_token_service_(auth_token_service),
     resource_name_(kDefaultResourceName),
     xmpp_client_(NULL),
     state_(DISCONNECTED),
     error_(OK) {
}

XmppSignalStrategy::~XmppSignalStrategy() {
  Disconnect();
}

void XmppSignalStrategy::Connect() {
  DCHECK(CalledOnValidThread());

  // Disconnect first if we are currently connected.
  Disconnect();

  buzz::XmppClientSettings settings;
  buzz::Jid login_jid(username_);
  settings.set_user(login_jid.node());
  settings.set_host(login_jid.domain());
  settings.set_resource(resource_name_);
  settings.set_use_tls(buzz::TLS_ENABLED);
  settings.set_token_service(auth_token_service_);
  settings.set_auth_cookie(auth_token_);
  settings.set_server(talk_base::SocketAddress("talk.google.com", 5222));

  buzz::AsyncSocket* socket = new XmppSocketAdapter(settings, false);

  xmpp_client_ = new buzz::XmppClient(thread_->task_pump());
  xmpp_client_->Connect(settings, "", socket, CreatePreXmppAuth(settings));
  xmpp_client_->SignalStateChange.connect(
      this, &XmppSignalStrategy::OnConnectionStateChanged);
  xmpp_client_->engine()->AddStanzaHandler(this, buzz::XmppEngine::HL_TYPE);
  xmpp_client_->Start();

  SetState(CONNECTING);
}

void XmppSignalStrategy::Disconnect() {
  DCHECK(CalledOnValidThread());

  if (xmpp_client_) {
    xmpp_client_->engine()->RemoveStanzaHandler(this);

    xmpp_client_->Disconnect();

    // |xmpp_client_| should be set to NULL in OnConnectionStateChanged()
    // in response to Disconnect() call above.
    DCHECK(xmpp_client_ == NULL);
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
    LOG(INFO) << "Dropping signalling message because XMPP "
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
    return "";
  }
  return xmpp_client_->NextId();
}

bool XmppSignalStrategy::HandleStanza(const buzz::XmlElement* stanza) {
  DCHECK(CalledOnValidThread());
  ObserverListBase<Listener>::Iterator it(listeners_);
  Listener* listener;
  while ((listener = it.GetNext()) != NULL) {
    if (listener->OnSignalStrategyIncomingStanza(stanza))
      return true;
  }
  return false;
}

void XmppSignalStrategy::SetAuthInfo(const std::string& username,
                                     const std::string& auth_token,
                                     const std::string& auth_token_service) {
  DCHECK(CalledOnValidThread());
  username_ = username;
  auth_token_ = auth_token;
  auth_token_service_ = auth_token_service;
}

void XmppSignalStrategy::SetResourceName(const std::string &resource_name) {
  DCHECK(CalledOnValidThread());
  resource_name_ = resource_name;
}

void XmppSignalStrategy::OnConnectionStateChanged(
    buzz::XmppEngine::State state) {
  DCHECK(CalledOnValidThread());

  if (state == buzz::XmppEngine::STATE_OPEN) {
    // Verify that the JID that we've received matches the username
    // that we have. If it doesn't, then the OAuth token was probably
    // issued for a different account, so we treat is a an auth error.
    //
    // TODO(sergeyu): Some user accounts may not have associated
    // e-mail address. The check below will fail for such
    // accounts. Make sure we can handle this case proprely.
    if (!StartsWithASCII(GetLocalJid(), username_, false)) {
      LOG(ERROR) << "Received JID that is different from the expected value.";
      error_ = AUTHENTICATION_FAILED;
      xmpp_client_->SignalStateChange.disconnect(this);
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&DisconnectXmppClient, xmpp_client_));
      xmpp_client_ = NULL;
      SetState(DISCONNECTED);
      return;
    }

    keep_alive_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kKeepAliveIntervalSeconds),
        this, &XmppSignalStrategy::SendKeepAlive);
    SetState(CONNECTED);
  } else if (state == buzz::XmppEngine::STATE_CLOSED) {
    // Make sure we dump errors to the log.
    int subcode;
    buzz::XmppEngine::Error error = xmpp_client_->GetError(&subcode);
    LOG(INFO) << "XMPP connection was closed: error=" << error
              << ", subcode=" << subcode;

    keep_alive_timer_.Stop();

    // Client is destroyed by the TaskRunner after the client is
    // closed. Reset the pointer so we don't try to use it later.
    xmpp_client_ = NULL;

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
  std::string mechanism = notifier::GaiaTokenPreXmppAuth::kDefaultAuthMechanism;
  if (settings.token_service() == "oauth2") {
    mechanism = "X-OAUTH2";
  }

  return new notifier::GaiaTokenPreXmppAuth(
      jid.Str(), settings.auth_cookie(), settings.token_service(), mechanism);
}

}  // namespace remoting
