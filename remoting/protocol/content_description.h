// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONTENT_DESCRIPTION_H_
#define REMOTING_PROTOCOL_CONTENT_DESCRIPTION_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "remoting/protocol/session_config.h"
#include "third_party/libjingle/source/talk/p2p/base/sessiondescription.h"

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {
namespace protocol {

// ContentDescription used for chromoting sessions. It contains the information
// from the content description stanza in the session intialization handshake.
//
// This class also provides a type abstraction so that the Chromotocol Session
// interface does not need to depend on libjingle.
class ContentDescription : public cricket::ContentDescription {
 public:
  ContentDescription(const CandidateSessionConfig* config,
                     const std::string& auth_token,
                     const std::string& certificate);
  virtual ~ContentDescription();

  const CandidateSessionConfig* config() const {
    return candidate_config_.get();
  }

  const std::string& auth_token() const { return auth_token_; }
  const std::string& certificate() const { return certificate_; }

  buzz::XmlElement* ToXml() const;

  static cricket::ContentDescription* ParseXml(const buzz::XmlElement* element);

 private:
  scoped_ptr<const CandidateSessionConfig> candidate_config_;

  // This may contain the initiating, or the accepting token depending on
  // context.
  std::string auth_token_;

  std::string certificate_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CONTENT_DESCRIPTION_H_
