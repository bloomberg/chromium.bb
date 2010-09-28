// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_chromoting_server.h"

#include "base/message_loop.h"
#include "remoting/base/constants.h"
#include "third_party/libjingle/source/talk/p2p/base/transport.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using cricket::ContentDescription;
using cricket::SessionDescription;
using cricket::Session;
using cricket::SessionManager;
using buzz::QName;

namespace remoting {

namespace {
const char kDescriptionTag[] =  "description";
const char kTypeTag[] = "type";
}  // namespace

ChromotingContentDescription::ChromotingContentDescription(
    const std::string& description)
    : description_(description) {
}

JingleChromotingServer::JingleChromotingServer(
    MessageLoop* message_loop)
    : message_loop_(message_loop),
      session_manager_(NULL),
      allow_local_ips_(false),
      closed_(false) {
  DCHECK(message_loop_);
}

void JingleChromotingServer::Init(
    const std::string& local_jid,
    cricket::SessionManager* session_manager,
    NewConnectionCallback* new_connection_callback) {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(
            this, &JingleChromotingServer::Init,
            local_jid, session_manager, new_connection_callback));
    return;
  }

  DCHECK(session_manager);
  DCHECK(new_connection_callback);

  local_jid_ = local_jid;
  new_connection_callback_.reset(new_connection_callback);
  session_manager_ = session_manager;
  session_manager_->AddClient(kChromotingXmlNamespace, this);
}

JingleChromotingServer::~JingleChromotingServer() {
  DCHECK(closed_);
}

void JingleChromotingServer::Close(Task* closed_task) {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &JingleChromotingServer::Close,
                                     closed_task));
    return;
  }

  if (!closed_) {
    // Close all connections.
    session_manager_->RemoveClient(kChromotingXmlNamespace);
    while (!connections_.empty()) {
      Session* session = connections_.front()->ReleaseSession();
      session_manager_->DestroySession(session);
      connections_.pop_front();
    }
    closed_ = true;
  }

  closed_task->Run();
  delete closed_task;
}

void JingleChromotingServer::set_allow_local_ips(bool allow_local_ips) {
  allow_local_ips_ = allow_local_ips;
}

scoped_refptr<ChromotingConnection> JingleChromotingServer::Connect(
    const std::string& jid,
    ChromotingConnection::StateChangeCallback* state_change_callback) {
  // Can be called from any thread.
  scoped_refptr<JingleChromotingConnection> connection =
      new JingleChromotingConnection(this);
  message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleChromotingServer::DoConnect,
                                   connection, jid, state_change_callback));
  return connection;
}

void JingleChromotingServer::DoConnect(
    scoped_refptr<JingleChromotingConnection> connection,
    const std::string& jid,
    ChromotingConnection::StateChangeCallback* state_change_callback) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  Session* session = session_manager_->CreateSession(
      local_jid_, kChromotingXmlNamespace);

  // Initialize connection object before we send initiate stanza.
  connection->SetStateChangeCallback(state_change_callback);
  connection->Init(session);
  connections_.push_back(connection);

  session->Initiate(jid, CreateSessionDescription());
}

MessageLoop* JingleChromotingServer::message_loop() {
  return message_loop_;
}

void JingleChromotingServer::OnSessionCreate(
    Session* session, bool incoming) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Allow local connections if neccessary.
  session->transport()->set_allow_local_ips(allow_local_ips_);

  // If this is an outcoming session, the the connection object is already
  // created.
  if (incoming) {
    JingleChromotingConnection* connection =
        new JingleChromotingConnection(this);
    connections_.push_back(connection);
    connection->Init(session);
  }
}

void JingleChromotingServer::OnSessionDestroy(Session* session) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  std::list<scoped_refptr<JingleChromotingConnection> >::iterator it;
  for (it = connections_.begin(); it != connections_.end(); ++it) {
    if ((*it)->HasSession(session)) {
      (*it)->ReleaseSession();
      connections_.erase(it);
      return;
    }
  }
}

void JingleChromotingServer::AcceptConnection(
    JingleChromotingConnection* connection,
    Session* session) {
  bool accept = false;
  // Always reject connection if we are closed or there is no callback.
  if (!closed_ && new_connection_callback_.get())
    new_connection_callback_->Run(connection, &accept);

  if (accept)
    session->Accept(CreateSessionDescription());
  else
    session->Reject();
}

bool JingleChromotingServer::ParseContent(
    const buzz::XmlElement* element,
    const cricket::ContentDescription** content,
    cricket::ParseError* error) {
  if (element->Name() == QName(kChromotingXmlNamespace, kDescriptionTag)) {
    const buzz::XmlElement* type_elem =
        element->FirstNamed(QName(kChromotingXmlNamespace, kTypeTag));
    if (type_elem != NULL) {
      *content = new ChromotingContentDescription(type_elem->BodyText());
      return true;
    }
  }
  LOG(ERROR) << "Invalid description: " << element->Str();
  return false;
}

// WriteContent creates content description for chromoting session. The
// description looks as follows:
//   <description xmlns="google:remoting">
//     <type>description_text</type>
//   </description>
// Currently description_text is always empty.
//
// TODO(sergeyu): Add more information to the content description. E.g.
// protocol version, etc.
bool JingleChromotingServer::WriteContent(
    const cricket::ContentDescription* content,
    buzz::XmlElement** elem,
    cricket::WriteError* error) {
  const ChromotingContentDescription* desc =
      static_cast<const ChromotingContentDescription*>(content);

  QName desc_tag(kChromotingXmlNamespace, kDescriptionTag);
  buzz::XmlElement* root = new buzz::XmlElement(desc_tag, true);
  QName type_tag(kChromotingXmlNamespace, kTypeTag);
  buzz::XmlElement* type_elem = new buzz::XmlElement(type_tag);
  type_elem->SetBodyText(desc->description());
  root->AddElement(type_elem);
  *elem = root;
  return true;
}

SessionDescription* JingleChromotingServer::CreateSessionDescription() {
  SessionDescription* desc = new SessionDescription();
  desc->AddContent("chromoting", kChromotingXmlNamespace,
                   new ChromotingContentDescription(""));
  return desc;
}

}  // namespace remoting
