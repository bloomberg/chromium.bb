// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_chromotocol_server.h"

#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "third_party/libjingle/source/talk/p2p/base/constants.h"
#include "third_party/libjingle/source/talk/p2p/base/transport.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using cricket::ContentDescription;
using cricket::SessionDescription;
using cricket::Session;
using cricket::SessionManager;
using buzz::QName;
using buzz::XmlElement;

namespace remoting {

namespace {

const char kDefaultNs[] = "";

const char kChromotingContentName[] = "chromoting";

// Following constants are used to format session description in XML.
const char kDescriptionTag[] = "description";
const char kControlTag[] = "control";
const char kEventTag[] = "event";
const char kVideoTag[] = "video";
const char kResolutionTag[] = "initial-resolution";

const char kTransportAttr[] = "transport";
const char kVersionAttr[] = "version";
const char kCodecAttr[] = "codec";
const char kWidthAttr[] = "width";
const char kHeightAttr[] = "height";

const char kStreamTransport[] = "stream";
const char kDatagramTransport[] = "datagram";
const char kSrtpTransport[] = "srtp";
const char kRtpDtlsTransport[] = "rtp-dtls";

const char kVp8Codec[] = "vp8";
const char kZipCodec[] = "zip";

const char* GetTransportName(ChannelConfig::TransportType type) {
  switch (type) {
    case ChannelConfig::TRANSPORT_STREAM:
      return kStreamTransport;
    case ChannelConfig::TRANSPORT_DATAGRAM:
      return kDatagramTransport;
    case ChannelConfig::TRANSPORT_SRTP:
      return kSrtpTransport;
    case ChannelConfig::TRANSPORT_RTP_DTLS:
      return kRtpDtlsTransport;
  }
  NOTREACHED();
  return NULL;
}

const char* GetCodecName(ChannelConfig::Codec type) {
  switch (type) {
    case ChannelConfig::CODEC_VP8:
      return kVp8Codec;
    case ChannelConfig::CODEC_ZIP:
      return kZipCodec;
    default:
      break;
  }
  NOTREACHED();
  return NULL;
}


// Format a channel configuration tag for chromotocol session description,
// e.g. for video channel:
//    <video transport="srtp" version="1" codec="vp8" />
XmlElement* FormatChannelConfig(const ChannelConfig& config,
                                const std::string& tag_name) {
  XmlElement* result = new XmlElement(
      QName(kChromotingXmlNamespace, tag_name));

  result->AddAttr(QName(kDefaultNs, kTransportAttr),
                   GetTransportName(config.transport));

  result->AddAttr(QName(kDefaultNs, kVersionAttr),
                  base::IntToString(config.version));

  if (config.codec != ChannelConfig::CODEC_UNDEFINED) {
    result->AddAttr(QName(kDefaultNs, kCodecAttr),
                    GetCodecName(config.codec));
  }

  return result;
}

bool ParseTransportName(const std::string& value,
                        ChannelConfig::TransportType* transport) {
  if (value == kStreamTransport) {
    *transport = ChannelConfig::TRANSPORT_STREAM;
  } else if (value == kDatagramTransport) {
    *transport = ChannelConfig::TRANSPORT_DATAGRAM;
  } else if (value == kSrtpTransport) {
    *transport = ChannelConfig::TRANSPORT_SRTP;
  } else if (value == kRtpDtlsTransport) {
    *transport = ChannelConfig::TRANSPORT_RTP_DTLS;
  } else {
    return false;
  }
  return true;
}

bool ParseCodecName(const std::string& value, ChannelConfig::Codec* codec) {
  if (value == kVp8Codec) {
    *codec = ChannelConfig::CODEC_VP8;
  } else if (value == kZipCodec) {
    *codec = ChannelConfig::CODEC_ZIP;
  } else {
    return false;
  }
  return true;
}

// Returns false if the element is invalid.
bool ParseChannelConfig(const XmlElement* element, bool codec_required,
                        ChannelConfig* config) {
  if (!ParseTransportName(element->Attr(QName(kDefaultNs, kTransportAttr)),
                          &config->transport) ||
      !base::StringToInt(element->Attr(QName(kDefaultNs, kVersionAttr)),
                         &config->version)) {
    return false;
  }

  if (codec_required) {
    if (!ParseCodecName(element->Attr(QName(kDefaultNs, kCodecAttr)),
                        &config->codec)) {
      return false;
    }
  } else {
    config->codec = ChannelConfig::CODEC_UNDEFINED;
  }

  return true;
}

}  // namespace

ChromotocolContentDescription::ChromotocolContentDescription(
    const CandidateChromotocolConfig* config)
    : candidate_config_(config) {
}

ChromotocolContentDescription::~ChromotocolContentDescription() { }

JingleChromotocolServer::JingleChromotocolServer(
    JingleThread* jingle_thread)
    : jingle_thread_(jingle_thread),
      session_manager_(NULL),
      allow_local_ips_(false),
      closed_(false) {
  DCHECK(jingle_thread_);
}

void JingleChromotocolServer::Init(
    const std::string& local_jid,
    cricket::SessionManager* session_manager,
    IncomingConnectionCallback* incoming_connection_callback) {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(
            this, &JingleChromotocolServer::Init,
            local_jid, session_manager, incoming_connection_callback));
    return;
  }

  DCHECK(session_manager);
  DCHECK(incoming_connection_callback);

  local_jid_ = local_jid;
  incoming_connection_callback_.reset(incoming_connection_callback);
  session_manager_ = session_manager;
  session_manager_->AddClient(kChromotingXmlNamespace, this);
}

JingleChromotocolServer::~JingleChromotocolServer() {
  DCHECK(closed_);
}

void JingleChromotocolServer::Close(Task* closed_task) {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &JingleChromotocolServer::Close,
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

void JingleChromotocolServer::set_allow_local_ips(bool allow_local_ips) {
  allow_local_ips_ = allow_local_ips;
}

scoped_refptr<ChromotocolConnection> JingleChromotocolServer::Connect(
    const std::string& jid,
    CandidateChromotocolConfig* chromotocol_config,
    ChromotocolConnection::StateChangeCallback* state_change_callback) {
  // Can be called from any thread.
  scoped_refptr<JingleChromotocolConnection> connection =
      new JingleChromotocolConnection(this);
  connection->set_candidate_config(chromotocol_config);
  message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleChromotocolServer::DoConnect,
                                   connection, jid,
                                   state_change_callback));
  return connection;
}

void JingleChromotocolServer::DoConnect(
    scoped_refptr<JingleChromotocolConnection> connection,
    const std::string& jid,
    ChromotocolConnection::StateChangeCallback* state_change_callback) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  Session* session = session_manager_->CreateSession(
      local_jid_, kChromotingXmlNamespace);

  // Initialize connection object before we send initiate stanza.
  connection->SetStateChangeCallback(state_change_callback);
  connection->Init(session);
  connections_.push_back(connection);

  session->Initiate(
      jid, CreateSessionDescription(connection->candidate_config()->Clone()));
}

JingleThread* JingleChromotocolServer::jingle_thread() {
  return jingle_thread_;
}

MessageLoop* JingleChromotocolServer::message_loop() {
  return jingle_thread_->message_loop();
}

void JingleChromotocolServer::OnSessionCreate(
    Session* session, bool incoming) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Allow local connections if neccessary.
  session->set_allow_local_ips(allow_local_ips_);

  // If this is an outcoming session the connection object is already
  // created.
  if (incoming) {
    JingleChromotocolConnection* connection =
        new JingleChromotocolConnection(this);
    connections_.push_back(connection);
    connection->Init(session);
  }
}

void JingleChromotocolServer::OnSessionDestroy(Session* session) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  std::list<scoped_refptr<JingleChromotocolConnection> >::iterator it;
  for (it = connections_.begin(); it != connections_.end(); ++it) {
    if ((*it)->HasSession(session)) {
      (*it)->ReleaseSession();
      connections_.erase(it);
      return;
    }
  }
}

void JingleChromotocolServer::AcceptConnection(
    JingleChromotocolConnection* connection,
    Session* session) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Reject connection if we are closed.
  if (closed_) {
    session->Reject(cricket::STR_TERMINATE_DECLINE);
    return;
  }

  const cricket::SessionDescription* session_description =
      session->remote_description();
  const cricket::ContentInfo* content =
      session_description->FirstContentByType(kChromotingXmlNamespace);

  CHECK(content);

  const ChromotocolContentDescription* content_description =
      static_cast<const ChromotocolContentDescription*>(content->description);
  connection->set_candidate_config(content_description->config()->Clone());

  IncomingConnectionResponse response = ChromotocolServer::DECLINE;

  // Always reject connection if there is no callback.
  if (incoming_connection_callback_.get())
    incoming_connection_callback_->Run(connection, &response);

  switch (response) {
    case ChromotocolServer::ACCEPT: {
      // Connection must be configured by the callback.
      DCHECK(connection->config());
      CandidateChromotocolConfig* candidate_config =
          CandidateChromotocolConfig::CreateFrom(connection->config());
      session->Accept(CreateSessionDescription(candidate_config));
      break;
    }

    case ChromotocolServer::INCOMPATIBLE: {
      session->Reject(cricket::STR_TERMINATE_INCOMPATIBLE_PARAMETERS);
      break;
    }

    case ChromotocolServer::DECLINE: {
      session->Reject(cricket::STR_TERMINATE_DECLINE);
      break;
    }

    default: {
      NOTREACHED();
    }
  }
}

// Parse content description generated by WriteContent().
bool JingleChromotocolServer::ParseContent(
    cricket::SignalingProtocol protocol,
    const XmlElement* element,
    const cricket::ContentDescription** content,
    cricket::ParseError* error) {
  if (element->Name() == QName(kChromotingXmlNamespace, kDescriptionTag)) {
    scoped_ptr<CandidateChromotocolConfig> config(
        CandidateChromotocolConfig::CreateEmpty());
    const XmlElement* child = NULL;

    // <control> tags.
    QName control_tag(kChromotingXmlNamespace, kControlTag);
    child = element->FirstNamed(control_tag);
    while (child) {
      ChannelConfig channel_config;
      if (!ParseChannelConfig(child, false, &channel_config))
        return false;
      config->AddControlConfig(channel_config);
      child = element->NextNamed(control_tag);
    }

    // <event> tags.
    QName event_tag(kChromotingXmlNamespace, kEventTag);
    child = element->FirstNamed(event_tag);
    while (child) {
      ChannelConfig channel_config;
      if (!ParseChannelConfig(child, false, &channel_config))
        return false;
      config->AddEventConfig(channel_config);
      child = element->NextNamed(event_tag);
    }

    // <video> tags.
    QName video_tag(kChromotingXmlNamespace, kVideoTag);
    child = element->FirstNamed(video_tag);
    while (child) {
      ChannelConfig channel_config;
      if (!ParseChannelConfig(child, true, &channel_config))
        return false;
      config->AddVideoConfig(channel_config);
      child = element->NextNamed(video_tag);
    }

    // <initial-resolution> tag.
    child = element->FirstNamed(QName(kChromotingXmlNamespace, kResolutionTag));
    if (!child)
      return false; // Resolution must always be specified.
    int width;
    int height;
    if (!base::StringToInt(child->Attr(QName(kDefaultNs, kWidthAttr)),
                           &width) ||
        !base::StringToInt(child->Attr(QName(kDefaultNs, kHeightAttr)),
                           &height)) {
      return false;
    }
    ScreenResolution resolution(width, height);
    if (!resolution.IsValid()) {
      return false;
    }

    config->SetInitialResolution(resolution);

    *content = new ChromotocolContentDescription(config.release());
    return true;
  }
  LOG(ERROR) << "Invalid description: " << element->Str();
  return false;
}

// WriteContent creates content description for chromoting session. The
// description looks as follows:
//   <description xmlns="google:remoting">
//     <control transport="stream" version="1" />
//     <event transport="datagram" version="1" />
//     <video transport="srtp" codec="vp8" version="1" />
//     <initial-resolution width="800" height="600" />
//   </description>
//
bool JingleChromotocolServer::WriteContent(
    cricket::SignalingProtocol protocol,
    const cricket::ContentDescription* content,
    XmlElement** elem,
    cricket::WriteError* error) {
  const ChromotocolContentDescription* desc =
      static_cast<const ChromotocolContentDescription*>(content);

  XmlElement* root = new XmlElement(
      QName(kChromotingXmlNamespace, kDescriptionTag), true);

  const CandidateChromotocolConfig* config = desc->config();
  std::vector<ChannelConfig>::const_iterator it;

  for (it = config->control_configs().begin();
       it != config->control_configs().end(); ++it) {
    root->AddElement(FormatChannelConfig(*it, kControlTag));
  }

  for (it = config->event_configs().begin();
       it != config->event_configs().end(); ++it) {
    root->AddElement(FormatChannelConfig(*it, kEventTag));
  }

  for (it = config->video_configs().begin();
       it != config->video_configs().end(); ++it) {
    root->AddElement(FormatChannelConfig(*it, kVideoTag));
  }

  XmlElement* resolution_tag = new XmlElement(
      QName(kChromotingXmlNamespace, kResolutionTag));
  resolution_tag->AddAttr(QName(kDefaultNs, kWidthAttr),
                          base::IntToString(
                              config->initial_resolution().width));
  resolution_tag->AddAttr(QName(kDefaultNs, kHeightAttr),
                          base::IntToString(
                              config->initial_resolution().height));
  root->AddElement(resolution_tag);

  *elem = root;
  return true;
}

SessionDescription* JingleChromotocolServer::CreateSessionDescription(
    const CandidateChromotocolConfig* config) {
  SessionDescription* desc = new SessionDescription();
  desc->AddContent(JingleChromotocolConnection::kChromotingContentName,
                   kChromotingXmlNamespace,
                   new ChromotocolContentDescription(config));
  return desc;
}

}  // namespace remoting
