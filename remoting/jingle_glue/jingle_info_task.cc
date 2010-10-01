// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/jingle_info_task.h"

#include "base/scoped_ptr.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"
#include "third_party/libjingle/source/talk/xmpp/xmppclient.h"

namespace remoting {

// This code is a copy of googleclient/talk/app/jingleinfotask.cc .

class JingleInfoTask::JingleInfoGetTask : public XmppTask {
 public:
  explicit JingleInfoGetTask(talk_base::TaskParent* parent)
      : XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
        done_(false) {
  }

  virtual int ProcessStart() {
    // Set jingle info query IQ stanza.
    scoped_ptr<buzz::XmlElement> get_iq(
        MakeIq(buzz::STR_GET, buzz::JID_EMPTY, task_id()));
    get_iq->AddElement(new buzz::XmlElement(buzz::QN_JINGLE_INFO_QUERY, true));
    if (SendStanza(get_iq.get()) != buzz::XMPP_RETURN_OK) {
      return STATE_ERROR;
    }
    return STATE_RESPONSE;
  }

  virtual int ProcessResponse() {
    if (done_) {
      return STATE_DONE;
    }
    return STATE_BLOCKED;
  }

 protected:
  virtual bool HandleStanza(const buzz::XmlElement* stanza) {
    if (!MatchResponseIq(stanza, buzz::JID_EMPTY, task_id())) {
      return false;
    }

    if (stanza->Attr(buzz::QN_TYPE) != buzz::STR_RESULT) {
      return false;
    }

    // Queue the stanza with the parent so these don't get handled out of order.
    JingleInfoTask* parent = static_cast<JingleInfoTask*>(GetParent());
    parent->QueueStanza(stanza);

    // Wake ourselves so we can go into the done state.
    done_ = true;
    Wake();
    return true;
  }

  bool done_;
};


JingleInfoTask::JingleInfoTask(talk_base::TaskParent* parent)
    : XmppTask(parent, buzz::XmppEngine::HL_TYPE) {
}

JingleInfoTask::~JingleInfoTask() {}

void JingleInfoTask::RefreshJingleInfoNow() {
  JingleInfoGetTask* get_task = new JingleInfoGetTask(this);
  get_task->Start();
}

bool JingleInfoTask::HandleStanza(const buzz::XmlElement* stanza) {
  if (!MatchRequestIq(stanza, "set", buzz::QN_JINGLE_INFO_QUERY)) {
    return false;
  }

  // Only respect relay push from the server.
  buzz::Jid from(stanza->Attr(buzz::QN_FROM));
  if (from != buzz::JID_EMPTY &&
      !from.BareEquals(GetClient()->jid()) &&
      from != buzz::Jid(GetClient()->jid().domain())) {
    return false;
  }

  QueueStanza(stanza);
  return true;
}

int JingleInfoTask::ProcessStart() {
  std::vector<std::string> relay_hosts;
  std::vector<talk_base::SocketAddress> stun_hosts;
  std::string relay_token;
  const buzz::XmlElement* stanza = NextStanza();
  if (stanza == NULL) {
    return STATE_BLOCKED;
  }
  const buzz::XmlElement* query =
      stanza->FirstNamed(buzz::QN_JINGLE_INFO_QUERY);
  if (query == NULL) {
    return STATE_START;
  }
  const buzz::XmlElement* stun = query->FirstNamed(buzz::QN_JINGLE_INFO_STUN);
  if (stun) {
    for (const buzz::XmlElement* server =
             stun->FirstNamed(buzz::QN_JINGLE_INFO_SERVER);
         server != NULL;
         server = server->NextNamed(buzz::QN_JINGLE_INFO_SERVER)) {
      std::string host = server->Attr(buzz::QN_JINGLE_INFO_HOST);
      std::string port = server->Attr(buzz::QN_JINGLE_INFO_UDP);
      if (host != buzz::STR_EMPTY && host != buzz::STR_EMPTY) {
        // TODO(sergeyu): Avoid atoi() here.
        stun_hosts.push_back(
            talk_base::SocketAddress(host, atoi(port.c_str())));
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
  SignalJingleInfo(relay_token, relay_hosts, stun_hosts);
  return STATE_START;
}

}  // namespace remoting
