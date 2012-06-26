// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/content_description.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/authenticator.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::QName;
using buzz::XmlElement;

namespace remoting {
namespace protocol {

const char ContentDescription::kChromotingContentName[] = "chromoting";

namespace {

const char kDefaultNs[] = "";

// Following constants are used to format session description in XML.
const char kDescriptionTag[] = "description";
const char kControlTag[] = "control";
const char kEventTag[] = "event";
const char kVideoTag[] = "video";
const char kAudioTag[] = "audio";
const char kDeprecatedResolutionTag[] = "initial-resolution";

const char kTransportAttr[] = "transport";
const char kVersionAttr[] = "version";
const char kCodecAttr[] = "codec";
const char kDeprecatedWidthAttr[] = "width";
const char kDeprecatedHeightAttr[] = "height";

const char kStreamTransport[] = "stream";
const char kDatagramTransport[] = "datagram";
const char kNoneTransport[] = "none";

const char kVerbatimCodec[] = "verbatim";
const char kVp8Codec[] = "vp8";
const char kZipCodec[] = "zip";
const char kVorbisCodec[] = "vorbis";

const char* GetTransportName(ChannelConfig::TransportType type) {
  switch (type) {
    case ChannelConfig::TRANSPORT_STREAM:
      return kStreamTransport;
    case ChannelConfig::TRANSPORT_DATAGRAM:
      return kDatagramTransport;
    case ChannelConfig::TRANSPORT_NONE:
      return kNoneTransport;
  }
  NOTREACHED();
  return NULL;
}

const char* GetCodecName(ChannelConfig::Codec type) {
  switch (type) {
    case ChannelConfig::CODEC_VERBATIM:
      return kVerbatimCodec;
    case ChannelConfig::CODEC_VP8:
      return kVp8Codec;
    case ChannelConfig::CODEC_ZIP:
      return kZipCodec;
    case ChannelConfig::CODEC_VORBIS:
      return kVorbisCodec;
    default:
      break;
  }
  NOTREACHED();
  return NULL;
}


// Format a channel configuration tag for chromotocol session description,
// e.g. for video channel:
//    <video transport="stream" version="1" codec="vp8" />
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
  } else if (value == kNoneTransport) {
    *transport = ChannelConfig::TRANSPORT_NONE;
  } else {
    return false;
  }
  return true;
}

bool ParseCodecName(const std::string& value, ChannelConfig::Codec* codec) {
  if (value == kVerbatimCodec) {
    *codec = ChannelConfig::CODEC_VERBATIM;
  } else if (value == kVp8Codec) {
    *codec = ChannelConfig::CODEC_VP8;
  } else if (value == kZipCodec) {
    *codec = ChannelConfig::CODEC_ZIP;
  } else if (value == kVorbisCodec) {
    *codec = ChannelConfig::CODEC_VORBIS;
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
    scoped_ptr<CandidateSessionConfig> config,
    scoped_ptr<buzz::XmlElement> authenticator_message)
    : candidate_config_(config.Pass()),
      authenticator_message_(authenticator_message.Pass()) {
}

ContentDescription::~ContentDescription() { }

ContentDescription* ContentDescription::Copy() const {
  if (!candidate_config_.get() || !authenticator_message_.get()) {
    return NULL;
  }
  scoped_ptr<XmlElement> message(new XmlElement(*authenticator_message_));
  return new ContentDescription(candidate_config_->Clone(), message.Pass());
}

// ToXml() creates content description for chromoting session. The
// description looks as follows:
//   <description xmlns="google:remoting">
//     <control transport="stream" version="1" />
//     <event transport="datagram" version="1" />
//     <video transport="stream" codec="vp8" version="1" />
//     <audio transport="stream" codec="vorbis" version="1" />
//     <authentication>
//      Message created by Authenticator implementation.
//     </authentication>
//   </description>
//
XmlElement* ContentDescription::ToXml() const {
  XmlElement* root = new XmlElement(
      QName(kChromotingXmlNamespace, kDescriptionTag), true);

  std::vector<ChannelConfig>::const_iterator it;

  for (it = config()->control_configs().begin();
       it != config()->control_configs().end(); ++it) {
    root->AddElement(FormatChannelConfig(*it, kControlTag));
  }

  for (it = config()->event_configs().begin();
       it != config()->event_configs().end(); ++it) {
    root->AddElement(FormatChannelConfig(*it, kEventTag));
  }

  for (it = config()->video_configs().begin();
       it != config()->video_configs().end(); ++it) {
    root->AddElement(FormatChannelConfig(*it, kVideoTag));
  }

  for (it = config()->audio_configs().begin();
       it != config()->audio_configs().end(); ++it) {
    root->AddElement(FormatChannelConfig(*it, kAudioTag));
  }

  // Older endpoints require an initial-resolution tag, but otherwise ignore it.
  XmlElement* resolution_tag = new XmlElement(
      QName(kChromotingXmlNamespace, kDeprecatedResolutionTag));
  resolution_tag->AddAttr(QName(kDefaultNs, kDeprecatedWidthAttr), "640");
  resolution_tag->AddAttr(QName(kDefaultNs, kDeprecatedHeightAttr), "480");
  root->AddElement(resolution_tag);

  if (authenticator_message_.get()) {
    DCHECK(Authenticator::IsAuthenticatorMessage(authenticator_message_.get()));
    root->AddElement(new XmlElement(*authenticator_message_));
  }

  return root;
}

// static
// Adds the channel configs corresponding to |tag_name|,
// found in |element|, to |configs|.
bool ContentDescription::ParseChannelConfigs(
    const XmlElement* const element,
    const char tag_name[],
    bool codec_required,
    bool optional,
    std::vector<ChannelConfig>* const configs) {

  QName tag(kChromotingXmlNamespace, tag_name);
  const XmlElement* child = element->FirstNamed(tag);
  while (child) {
    ChannelConfig channel_config;
    if (!ParseChannelConfig(child, codec_required, &channel_config))
      return false;
    configs->push_back(channel_config);
    child = child->NextNamed(tag);
  }
  if (optional && configs->empty()) {
      // If there's no mention of the tag, implicitly assume
      // TRANSPORT_NONE for the channel.
      configs->push_back(ChannelConfig(ChannelConfig::TRANSPORT_NONE,
                                       kDefaultStreamVersion,
                                       ChannelConfig::CODEC_VERBATIM));
  }
  return true;
}

// static
ContentDescription* ContentDescription::ParseXml(
    const XmlElement* element) {
  if (element->Name() == QName(kChromotingXmlNamespace, kDescriptionTag)) {
    scoped_ptr<CandidateSessionConfig> config(
        CandidateSessionConfig::CreateEmpty());
    const XmlElement* child = NULL;

    if (!ParseChannelConfigs(element, kControlTag, false, false,
                             config->mutable_control_configs())) {
      return NULL;
    }
    if (!ParseChannelConfigs(element, kEventTag, false, false,
                             config->mutable_event_configs())) {
      return NULL;
    }
    if (!ParseChannelConfigs(element, kVideoTag, true, false,
                             config->mutable_video_configs())) {
      return NULL;
    }
    if (!ParseChannelConfigs(element, kAudioTag, true, true,
                             config->mutable_audio_configs())) {
      return NULL;
    }

    scoped_ptr<XmlElement> authenticator_message;
    child = Authenticator::FindAuthenticatorMessage(element);
    if (child)
      authenticator_message.reset(new XmlElement(*child));

    return new ContentDescription(config.Pass(), authenticator_message.Pass());
  }
  LOG(ERROR) << "Invalid description: " << element->Str();
  return NULL;
}

}  // namespace protocol
}  // namespace remoting
