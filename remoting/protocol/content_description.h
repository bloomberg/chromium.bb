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
  static const char kChromotingContentName[];

  // Takes ownership of |config| and |authenticator_message|.
  ContentDescription(const CandidateSessionConfig* config,
                     const buzz::XmlElement* authenticator_message);
  virtual ~ContentDescription();

  const CandidateSessionConfig* config() const {
    return candidate_config_.get();
  }

  const buzz::XmlElement* authenticator_message() const {
    return authenticator_message_.get();
  }

  buzz::XmlElement* ToXml() const;

  static ContentDescription* ParseXml(const buzz::XmlElement* element);

 private:
  scoped_ptr<const CandidateSessionConfig> candidate_config_;
  scoped_ptr<const buzz::XmlElement> authenticator_message_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CONTENT_DESCRIPTION_H_
