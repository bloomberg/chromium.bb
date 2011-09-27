// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_MESSAGES_H_
#define REMOTING_PROTOCOL_JINGLE_MESSAGES_H_

#include <list>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace cricket {
class Candidate;
}  // namespace cricket

namespace remoting {
namespace protocol {

class ContentDescription;

extern const char kJabberNamespace[];
extern const char kJingleNamespace[];
extern const char kP2PTransportNamespace[];

struct JingleMessage {
  enum ActionType {
    UNKNOWN_ACTION,
    SESSION_INITIATE,
    SESSION_ACCEPT,
    SESSION_TERMINATE,
    TRANSPORT_INFO,
  };

  enum Reason {
    // Currently only termination reasons that can be sent by the host
    // are understood. All others are converted to UNKNOWN_REASON.
    UNKNOWN_REASON,
    SUCCESS,
    DECLINE,
    INCOMPATIBLE_PARAMETERS,
  };

  JingleMessage();
  JingleMessage(const std::string& to_value,
                ActionType action_value,
                const std::string& sid_value);
  ~JingleMessage();

  // Caller keeps ownership of |stanza|.
  static bool IsJingleMessage(const buzz::XmlElement* stanza);

  // Caller keeps ownership of |stanza|. |error| is set to debug error
  // message when parsing fails.
  bool ParseXml(const buzz::XmlElement* stanza, std::string* error);

  buzz::XmlElement* ToXml();

  std::string from;
  std::string to;
  ActionType action;
  std::string sid;

  scoped_ptr<ContentDescription> description;
  std::list<cricket::Candidate> candidates;

  // Value from the <reason> tag if it is present in the
  // message. Useful mainly for session-terminate messages, but Jingle
  // spec allows it in any message.
  Reason reason;
};

struct JingleMessageReply {
  enum ReplyType {
    REPLY_RESULT,
    REPLY_ERROR,
  };
  enum ErrorType {
    NONE,
    BAD_REQUEST,
    NOT_IMPLEMENTED,
    INVALID_SID,
    UNEXPECTED_REQUEST,
  };

  JingleMessageReply();
  JingleMessageReply(ErrorType error);
  JingleMessageReply(ErrorType error, const std::string& text);
  ~JingleMessageReply();

  // Formats reply stanza for the specified |request_stanza|. Id and
  // recepient as well as other information needed to generate a valid
  // reply are taken from |request_stanza|.
  buzz::XmlElement* ToXml(const buzz::XmlElement* request_stanza) const;

  ReplyType type;
  ErrorType error_type;
  std::string text;
};

}  // protocol
}  // remoting

#endif  // REMOTING_PROTOCOL_JINGLE_MESSAGES_H_
