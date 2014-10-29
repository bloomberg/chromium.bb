// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_MESSAGES_H_
#define REMOTING_PROTOCOL_JINGLE_MESSAGES_H_

#include <list>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "third_party/libjingle/source/talk/p2p/base/candidate.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"


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
    SESSION_INFO,
    TRANSPORT_INFO,
  };

  enum Reason {
    UNKNOWN_REASON,
    SUCCESS,
    DECLINE,
    CANCEL,
    GENERAL_ERROR,
    INCOMPATIBLE_PARAMETERS,
  };

  struct NamedCandidate {
    NamedCandidate();
    NamedCandidate(const std::string& name,
                   const cricket::Candidate& candidate);

    std::string name;
    cricket::Candidate candidate;
  };

  JingleMessage();
  JingleMessage(const std::string& to_value,
                ActionType action_value,
                const std::string& sid_value);
  ~JingleMessage();

  // Caller keeps ownership of |stanza|.
  static bool IsJingleMessage(const buzz::XmlElement* stanza);
  static std::string GetActionName(ActionType action);

  // Caller keeps ownership of |stanza|. |error| is set to debug error
  // message when parsing fails.
  bool ParseXml(const buzz::XmlElement* stanza, std::string* error);

  scoped_ptr<buzz::XmlElement> ToXml() const;

  std::string from;
  std::string to;
  ActionType action;
  std::string sid;

  std::string initiator;

  scoped_ptr<ContentDescription> description;
  std::list<NamedCandidate> candidates;

  // Content of session-info messages.
  scoped_ptr<buzz::XmlElement> info;

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
    UNSUPPORTED_INFO,
  };

  JingleMessageReply();
  JingleMessageReply(ErrorType error);
  JingleMessageReply(ErrorType error, const std::string& text);
  ~JingleMessageReply();

  // Formats reply stanza for the specified |request_stanza|. Id and
  // recepient as well as other information needed to generate a valid
  // reply are taken from |request_stanza|.
  scoped_ptr<buzz::XmlElement> ToXml(
      const buzz::XmlElement* request_stanza) const;

  ReplyType type;
  ErrorType error_type;
  std::string text;
};

}  // protocol
}  // remoting

#endif  // REMOTING_PROTOCOL_JINGLE_MESSAGES_H_
