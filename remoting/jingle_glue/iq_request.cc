// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/iq_request.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "third_party/libjingle/source/talk/p2p/base/sessionmanager.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"
#include "third_party/libjingle/source/talk/xmpp/xmppclient.h"

namespace remoting {

// static
buzz::XmlElement* IqRequest::MakeIqStanza(const std::string& type,
                                          const std::string& addressee,
                                          buzz::XmlElement* iq_body,
                                          const std::string& id) {
  buzz::XmlElement* stanza = new buzz::XmlElement(buzz::QN_IQ);
  stanza->AddAttr(buzz::QN_TYPE, type);
  stanza->AddAttr(buzz::QN_TO, addressee);
  stanza->AddAttr(buzz::QN_ID, id);
  stanza->AddElement(iq_body);
  return stanza;
}

XmppIqRequest::XmppIqRequest(MessageLoop* message_loop,
                             buzz::XmppClient* xmpp_client)
    : message_loop_(message_loop),
      xmpp_client_(xmpp_client),
      cookie_(NULL) {
  DCHECK(xmpp_client_);
  DCHECK_EQ(MessageLoop::current(), message_loop_);
}

XmppIqRequest::~XmppIqRequest() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  Unregister();
}

void XmppIqRequest::SendIq(const std::string& type,
                       const std::string& addressee,
                       buzz::XmlElement* iq_body) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // Unregister the handler if it is already registered.
  Unregister();

  DCHECK_GT(type.length(), 0U);

  scoped_ptr<buzz::XmlElement> stanza(MakeIqStanza(type, addressee, iq_body,
                                                   xmpp_client_->NextId()));

  xmpp_client_->engine()->SendIq(stanza.get(), this, &cookie_);
}

void XmppIqRequest::set_callback(ReplyCallback* callback) {
  callback_.reset(callback);
}

void XmppIqRequest::Unregister() {
  if (cookie_) {
    // No need to unregister the handler if the client has been destroyed.
    if (xmpp_client_) {
      xmpp_client_->engine()->RemoveIqHandler(cookie_, NULL);
    }
    cookie_ = NULL;
  }
}

void XmppIqRequest::IqResponse(buzz::XmppIqCookie cookie,
                               const buzz::XmlElement* stanza) {
  if (callback_.get() != NULL) {
    callback_->Run(stanza);
  }
}

JavascriptIqRegistry::JavascriptIqRegistry()
   : current_id_(0),
     default_handler_(NULL) {
}

JavascriptIqRegistry::~JavascriptIqRegistry() {
}

void JavascriptIqRegistry::RemoveAllRequests(JavascriptIqRequest* request) {
  IqRequestMap::iterator it = requests_.begin();
  while (it != requests_.end()) {
    IqRequestMap::iterator cur = it;
    ++it;
    if (cur->second == request) {
      requests_.erase(cur);
    }
  }
}

void JavascriptIqRegistry::SetDefaultHandler(JavascriptIqRequest* new_handler) {
  // Should only allow a new handler if |default_handler_| is NULL.
  CHECK(default_handler_ == NULL || !new_handler);
  default_handler_ = new_handler;
}

void JavascriptIqRegistry::OnIq(const std::string& response_xml) {
  // TODO(ajwong): Can we cleanup this dispatch at all?  The send is from
  // JavascriptIqRequest but the return is in JavascriptIqRegistry.
  scoped_ptr<buzz::XmlElement> stanza(buzz::XmlElement::ForStr(response_xml));

  if (!stanza.get()) {
    LOG(WARNING) << "Malformed XML received" << response_xml;
    return;
  }

  LOG(ERROR) << "IQ Received: " << stanza->Str();
  if (stanza->Name() != buzz::QN_IQ) {
    LOG(WARNING) << "Received unexpected non-IQ packet" << stanza->Str();
    return;
  }

  if (!stanza->HasAttr(buzz::QN_ID)) {
    LOG(WARNING) << "IQ packet missing id" << stanza->Str();
    return;
  }

  const std::string& id = stanza->Attr(buzz::QN_ID);

  IqRequestMap::iterator it = requests_.find(id);
  if (it == requests_.end()) {
    if (!default_handler_) {
      VLOG(1) << "Dropping IQ packet with no request id: " << stanza->Str();
    } else {
      if (default_handler_->callback_.get()) {
        default_handler_->callback_->Run(stanza.get());
      } else {
        VLOG(1) << "default handler has no callback, so dropping: "
                << stanza->Str();
      }
    }
  } else {
    // TODO(ajwong): We should look at the logic inside libjingle's
    // XmppTask::MatchResponseIq() and make sure we're fully in sync.
    // They check more fields and conditions than us.

    // TODO(ajwong): This logic is weird.  We add to the register in
    // JavascriptIqRequest::SendIq(), but remove in
    // JavascriptIqRegistry::OnIq().  We should try to keep the
    // registration/deregistration in one spot.
    if (it->second->callback_.get()) {
      it->second->callback_->Run(stanza.get());
    } else {
      VLOG(1) << "No callback, so dropping: " << stanza->Str();
    }
    requests_.erase(it);
  }
}

std::string JavascriptIqRegistry::RegisterRequest(
    JavascriptIqRequest* request) {
  ++current_id_;
  std::string id_as_string = base::IntToString(current_id_);

  requests_[id_as_string] = request;
  return id_as_string;
}

JavascriptIqRequest::JavascriptIqRequest(JavascriptIqRegistry* registry,
                                         scoped_refptr<XmppProxy> xmpp_proxy)
    : xmpp_proxy_(xmpp_proxy),
      registry_(registry),
      is_default_handler_(false) {
}

JavascriptIqRequest::~JavascriptIqRequest() {
  registry_->RemoveAllRequests(this);
  if (is_default_handler_) {
    registry_->SetDefaultHandler(NULL);
  }
}

void JavascriptIqRequest::SendIq(const std::string& type,
                                 const std::string& addressee,
                                 buzz::XmlElement* iq_body) {
  scoped_ptr<buzz::XmlElement> stanza(
      MakeIqStanza(type, addressee, iq_body,
                   registry_->RegisterRequest(this)));

  xmpp_proxy_->SendIq(stanza->Str());
}

void JavascriptIqRequest::SendRawIq(buzz::XmlElement* stanza) {
  xmpp_proxy_->SendIq(stanza->Str());
}

void JavascriptIqRequest::BecomeDefaultHandler() {
  is_default_handler_ = true;
  registry_->SetDefaultHandler(this);
}

void JavascriptIqRequest::set_callback(ReplyCallback* callback) {
  callback_.reset(callback);
}

JingleInfoRequest::JingleInfoRequest(IqRequest* request)
    : request_(request) {
  request_->set_callback(NewCallback(this, &JingleInfoRequest::OnResponse));
}

JingleInfoRequest::~JingleInfoRequest() {
}

void JingleInfoRequest::Run(Task* done) {
  done_cb_.reset(done);
  request_->SendIq(buzz::STR_GET, buzz::STR_EMPTY,
                   new buzz::XmlElement(buzz::QN_JINGLE_INFO_QUERY, true));
}

void JingleInfoRequest::SetCallback(OnJingleInfoCallback* callback) {
  on_jingle_info_cb_.reset(callback);
}

void JingleInfoRequest::DetachCallback() {
  on_jingle_info_cb_.reset();
}

void JingleInfoRequest::OnResponse(const buzz::XmlElement* stanza) {
  std::vector<std::string> relay_hosts;
  std::vector<talk_base::SocketAddress> stun_hosts;
  std::string relay_token;

  const buzz::XmlElement* query =
      stanza->FirstNamed(buzz::QN_JINGLE_INFO_QUERY);
  if (query == NULL) {
    LOG(WARNING) << "No Jingle info found in Jingle Info query response."
                 << stanza->Str();
    return;
  }

  const buzz::XmlElement* stun = query->FirstNamed(buzz::QN_JINGLE_INFO_STUN);
  if (stun) {
    for (const buzz::XmlElement* server =
         stun->FirstNamed(buzz::QN_JINGLE_INFO_SERVER);
         server != NULL;
         server = server->NextNamed(buzz::QN_JINGLE_INFO_SERVER)) {
      std::string host = server->Attr(buzz::QN_JINGLE_INFO_HOST);
      std::string port_str = server->Attr(buzz::QN_JINGLE_INFO_UDP);
      if (host != buzz::STR_EMPTY && host != buzz::STR_EMPTY) {
        int port;
        if (!base::StringToInt(port_str, &port)) {
          LOG(WARNING) << "Unable to parse port in stanza" << stanza->Str();
        } else {
          stun_hosts.push_back(talk_base::SocketAddress(host, port));
        }
      }
    }
  }

  const buzz::XmlElement* relay = query->FirstNamed(buzz::QN_JINGLE_INFO_RELAY);
  if (relay) {
    relay_token = relay->TextNamed(buzz::QN_JINGLE_INFO_TOKEN);
    for (const buzz::XmlElement* server =
         relay->FirstNamed(buzz::QN_JINGLE_INFO_SERVER);
         server != NULL;
         server = server->NextNamed(buzz::QN_JINGLE_INFO_SERVER)) {
      std::string host = server->Attr(buzz::QN_JINGLE_INFO_HOST);
      if (host != buzz::STR_EMPTY) {
        relay_hosts.push_back(host);
      }
    }
  }

  if (on_jingle_info_cb_.get()) {
    on_jingle_info_cb_->Run(relay_token, relay_hosts, stun_hosts);
  } else {
    LOG(INFO) << "Iq reply parsed with no callback. Dropping" << stanza->Str();
  }

  DetachCallback();
  done_cb_->Run();
}

SessionStartRequest::SessionStartRequest(
    JavascriptIqRequest* request,
    cricket::SessionManager* session_manager)
    : request_(request),
      session_manager_(session_manager) {
  request_->set_callback(NewCallback(this, &SessionStartRequest::OnResponse));
}

SessionStartRequest::~SessionStartRequest() {
}

void SessionStartRequest::Run() {
  session_manager_->SignalOutgoingMessage.connect(
      this, &SessionStartRequest::OnOutgoingMessage);

  // TODO(ajwong): Why are we connecting SessionManager to itself?
  session_manager_->SignalRequestSignaling.connect(
      session_manager_, &cricket::SessionManager::OnSignalingReady);
  request_->BecomeDefaultHandler();
}

void SessionStartRequest::OnResponse(const buzz::XmlElement* response) {
  // TODO(ajwong): Techncially, when SessionManager sends IQ packets, it
  // actually expects a response in SessionSendTask(). However, if you look in
  // SessionManager::OnIncomingResponse(), it does nothing with the response.
  // Also, if no response is found, we are supposed to call
  // SessionManager::OnFailedSend().
  //
  // However, for right now, we just ignore those, and only propogate
  // messages outside of the request/reply framework to
  // SessionManager::OnIncomingMessage.

  if (session_manager_->IsSessionMessage(response)) {
    session_manager_->OnIncomingMessage(response);
  }
}

void SessionStartRequest::OnOutgoingMessage(
    cricket::SessionManager* session_manager,
    const buzz::XmlElement* stanza) {
  // TODO(ajwong): Are we just supposed to not use |session_manager|?
  DCHECK_EQ(session_manager, session_manager_);
  scoped_ptr<buzz::XmlElement> stanza_copy(new buzz::XmlElement(*stanza));
  request_->SendRawIq(stanza_copy.get());
}

}  // namespace remoting
