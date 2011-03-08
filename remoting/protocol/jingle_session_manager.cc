// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "remoting/protocol/jingle_session_manager.h"

#include "base/base64.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/proto/auth.pb.h"
#include "third_party/libjingle/source/talk/p2p/base/constants.h"
#include "third_party/libjingle/source/talk/p2p/base/transport.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::QName;
using buzz::XmlElement;

namespace remoting {
namespace protocol {

namespace {

const char kDefaultNs[] = "";

const char kChromotingContentName[] = "chromoting";

// Following constants are used to format session description in XML.
const char kDescriptionTag[] = "description";
const char kControlTag[] = "control";
const char kEventTag[] = "event";
const char kVideoTag[] = "video";
const char kResolutionTag[] = "initial-resolution";
const char kAuthenticationTag[] = "authentication";
const char kCertificateTag[] = "certificate";

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

ContentDescription::ContentDescription(
    const CandidateSessionConfig* candidate_config,
    const std::string& auth_token,
    scoped_refptr<net::X509Certificate> certificate)
    : candidate_config_(candidate_config),
      auth_token_(auth_token),
      certificate_(certificate) {
}

ContentDescription::~ContentDescription() { }

JingleSessionManager::JingleSessionManager(
    JingleThread* jingle_thread)
    : jingle_thread_(jingle_thread),
      cricket_session_manager_(NULL),
      allow_local_ips_(false),
      closed_(false) {
  DCHECK(jingle_thread_);
}

void JingleSessionManager::Init(
    const std::string& local_jid,
    cricket::SessionManager* cricket_session_manager,
    IncomingSessionCallback* incoming_session_callback,
    base::RSAPrivateKey* private_key,
    scoped_refptr<net::X509Certificate> certificate) {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(
            this, &JingleSessionManager::Init,
            local_jid, cricket_session_manager, incoming_session_callback,
            private_key, certificate));
    return;
  }

  DCHECK(cricket_session_manager);
  DCHECK(incoming_session_callback);

  local_jid_ = local_jid;
  certificate_ = certificate;
  private_key_.reset(private_key);
  incoming_session_callback_.reset(incoming_session_callback);
  cricket_session_manager_ = cricket_session_manager;
  cricket_session_manager_->AddClient(kChromotingXmlNamespace, this);
}

JingleSessionManager::~JingleSessionManager() {
  DCHECK(closed_);
}

void JingleSessionManager::Close(Task* closed_task) {
  if (MessageLoop::current() != message_loop()) {
    message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &JingleSessionManager::Close,
                                     closed_task));
    return;
  }

  if (!closed_) {
    // Close all connections.
    cricket_session_manager_->RemoveClient(kChromotingXmlNamespace);
    while (!sessions_.empty()) {
      cricket::Session* session = sessions_.front()->ReleaseSession();
      cricket_session_manager_->DestroySession(session);
      sessions_.pop_front();
    }
    closed_ = true;
  }

  closed_task->Run();
  delete closed_task;
}

void JingleSessionManager::set_allow_local_ips(bool allow_local_ips) {
  allow_local_ips_ = allow_local_ips;
}

scoped_refptr<protocol::Session> JingleSessionManager::Connect(
    const std::string& host_jid,
    const std::string& receiver_token,
    CandidateSessionConfig* candidate_config,
    protocol::Session::StateChangeCallback* state_change_callback) {
  // Can be called from any thread.
  scoped_refptr<JingleSession> jingle_session(
      JingleSession::CreateClientSession(this));
  jingle_session->set_candidate_config(candidate_config);
  jingle_session->set_receiver_token(receiver_token);
  message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleSessionManager::DoConnect,
                                   jingle_session, host_jid, receiver_token,
                                   state_change_callback));
  return jingle_session;
}

void JingleSessionManager::DoConnect(
    scoped_refptr<JingleSession> jingle_session,
    const std::string& host_jid,
    const std::string& receiver_token,
    protocol::Session::StateChangeCallback* state_change_callback) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  cricket::Session* cricket_session = cricket_session_manager_->CreateSession(
      local_jid_, kChromotingXmlNamespace);

  // Initialize connection object before we send initiate stanza.
  jingle_session->SetStateChangeCallback(state_change_callback);
  jingle_session->Init(cricket_session);
  sessions_.push_back(jingle_session);

  cricket_session->Initiate(
      host_jid,
      CreateSessionDescription(jingle_session->candidate_config()->Clone(),
                               receiver_token, NULL));
}

JingleThread* JingleSessionManager::jingle_thread() {
  return jingle_thread_;
}

MessageLoop* JingleSessionManager::message_loop() {
  return jingle_thread_->message_loop();
}

void JingleSessionManager::OnSessionCreate(
    cricket::Session* cricket_session, bool incoming) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Allow local connections if neccessary.
  cricket_session->set_allow_local_ips(allow_local_ips_);

  // If this is an outcoming session the session object is already created.
  if (incoming) {
    DCHECK(certificate_);
    DCHECK(private_key_.get());
    JingleSession* jingle_session =
        JingleSession::CreateServerSession(this, certificate_,
                                           private_key_.get());
    sessions_.push_back(make_scoped_refptr(jingle_session));
    jingle_session->Init(cricket_session);
  }
}

void JingleSessionManager::OnSessionDestroy(cricket::Session* cricket_session) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  std::list<scoped_refptr<JingleSession> >::iterator it;
  for (it = sessions_.begin(); it != sessions_.end(); ++it) {
    if ((*it)->HasSession(cricket_session)) {
      (*it)->ReleaseSession();
      sessions_.erase(it);
      return;
    }
  }
}

void JingleSessionManager::AcceptConnection(
    JingleSession* jingle_session,
    cricket::Session* cricket_session) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Reject connection if we are closed.
  if (closed_) {
    cricket_session->Reject(cricket::STR_TERMINATE_DECLINE);
    return;
  }

  const cricket::SessionDescription* session_description =
      cricket_session->remote_description();
  const cricket::ContentInfo* content =
      session_description->FirstContentByType(kChromotingXmlNamespace);

  CHECK(content);

  const ContentDescription* content_description =
      static_cast<const ContentDescription*>(content->description);
  jingle_session->set_candidate_config(content_description->config()->Clone());

  // Always reject connection if there is no callback.
  IncomingSessionResponse response = protocol::SessionManager::DECLINE;

  // Use the callback to generate a response.
  if (incoming_session_callback_.get())
    incoming_session_callback_->Run(jingle_session, &response);

  switch (response) {
    case protocol::SessionManager::ACCEPT: {
      // Connection must be configured by the callback.
      DCHECK(jingle_session->config());
      CandidateSessionConfig* candidate_config =
          CandidateSessionConfig::CreateFrom(jingle_session->config());
      cricket_session->Accept(
          CreateSessionDescription(candidate_config,
                                   jingle_session->initiator_token(),
                                   jingle_session->server_certificate()));
      break;
    }

    case protocol::SessionManager::INCOMPATIBLE: {
      cricket_session->Reject(cricket::STR_TERMINATE_INCOMPATIBLE_PARAMETERS);
      break;
    }

    case protocol::SessionManager::DECLINE: {
      cricket_session->Reject(cricket::STR_TERMINATE_DECLINE);
      break;
    }

    default: {
      NOTREACHED();
    }
  }
}

// Parse content description generated by WriteContent().
bool JingleSessionManager::ParseContent(
    cricket::SignalingProtocol protocol,
    const XmlElement* element,
    const cricket::ContentDescription** content,
    cricket::ParseError* error) {
  if (element->Name() == QName(kChromotingXmlNamespace, kDescriptionTag)) {
    scoped_ptr<CandidateSessionConfig> config(
        CandidateSessionConfig::CreateEmpty());
    const XmlElement* child = NULL;

    // <control> tags.
    QName control_tag(kChromotingXmlNamespace, kControlTag);
    child = element->FirstNamed(control_tag);
    while (child) {
      ChannelConfig channel_config;
      if (!ParseChannelConfig(child, false, &channel_config))
        return false;
      config->mutable_control_configs()->push_back(channel_config);
      child = child->NextNamed(control_tag);
    }

    // <event> tags.
    QName event_tag(kChromotingXmlNamespace, kEventTag);
    child = element->FirstNamed(event_tag);
    while (child) {
      ChannelConfig channel_config;
      if (!ParseChannelConfig(child, false, &channel_config))
        return false;
      config->mutable_event_configs()->push_back(channel_config);
      child = child->NextNamed(event_tag);
    }

    // <video> tags.
    QName video_tag(kChromotingXmlNamespace, kVideoTag);
    child = element->FirstNamed(video_tag);
    while (child) {
      ChannelConfig channel_config;
      if (!ParseChannelConfig(child, true, &channel_config))
        return false;
      config->mutable_video_configs()->push_back(channel_config);
      child = child->NextNamed(video_tag);
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

    *config->mutable_initial_resolution() = resolution;

    std::string auth_token;
    // TODO(ajwong): Parse this out.

    // Parse the certificate.
    scoped_refptr<net::X509Certificate> certificate;
    child = element->FirstNamed(QName(kChromotingXmlNamespace,
                                      kAuthenticationTag));
    if (child)
      child = child->FirstNamed(QName(kChromotingXmlNamespace,
                                      kCertificateTag));

    if (child) {
      std::string base64_cert = child->BodyText();
      std::string der_cert;
      bool ret = base::Base64Decode(base64_cert, &der_cert);
      if (!ret) {
        LOG(ERROR) << "Failed to decode certificate received from the peer.";
        return false;
      }
      certificate = net::X509Certificate::CreateFromBytes(der_cert.data(),
                                                          der_cert.length());
    }

    *content = new ContentDescription(config.release(), auth_token,
                                      certificate);
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
//     <authentication>
//       <certificate>[BASE64 Encoded Certificate]</certificate>
//     </authentication>" />
//   </description>
//
bool JingleSessionManager::WriteContent(
    cricket::SignalingProtocol protocol,
    const cricket::ContentDescription* content,
    XmlElement** elem,
    cricket::WriteError* error) {
  const ContentDescription* desc =
      static_cast<const ContentDescription*>(content);

  XmlElement* root = new XmlElement(
      QName(kChromotingXmlNamespace, kDescriptionTag), true);

  const CandidateSessionConfig* config = desc->config();
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

  if (desc->certificate()) {
    XmlElement* authentication_tag = new XmlElement(
        QName(kChromotingXmlNamespace, kAuthenticationTag));
    XmlElement* certificate_tag = new XmlElement(
        QName(kChromotingXmlNamespace, kCertificateTag));

    std::string der_cert;
    bool ret = desc->certificate()->GetDEREncoded(&der_cert);
    DCHECK(ret) << "Cannot obtain DER encoded certificate";

    std::string base64_cert;
    ret = base::Base64Encode(der_cert, &base64_cert);
    DCHECK(ret) << "Cannot perform base64 encode on certificate";

    certificate_tag->SetBodyText(base64_cert);
    authentication_tag->AddElement(certificate_tag);
    root->AddElement(authentication_tag);
  }

  *elem = root;
  return true;
}

cricket::SessionDescription* JingleSessionManager::CreateSessionDescription(
    const CandidateSessionConfig* config,
    const std::string& auth_token,
    scoped_refptr<net::X509Certificate> certificate) {
  cricket::SessionDescription* desc = new cricket::SessionDescription();
  desc->AddContent(JingleSession::kChromotingContentName,
                   kChromotingXmlNamespace,
                   new ContentDescription(config, auth_token, certificate));
  return desc;
}

}  // namespace protocol
}  // namespace remoting
