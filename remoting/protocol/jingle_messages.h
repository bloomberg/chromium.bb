// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_MESSAGES_H_
#define REMOTING_PROTOCOL_JINGLE_MESSAGES_H_

#include <list>
#include <memory>
#include <string>

#include "remoting/protocol/errors.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"
#include "third_party/webrtc/p2p/base/candidate.h"

namespace remoting {
namespace protocol {

class ContentDescription;

// Represents an address of a Chromoting endpoint and its routing channel.
// TODO(kelvinp): Move the struct to remoting/signaling. Potentially we could
// update SignalStrategy interface to use this instead of jid for addressing.
struct SignalingAddress {
  enum class Channel { LCS, XMPP };

  SignalingAddress();
  SignalingAddress(const std::string& jid);
  SignalingAddress(const std::string& jid,
                   const std::string& endpoint_id,
                   Channel channel);

  // Represents the |to| or |from| field in an IQ stanza.
  std::string jid;

  // Represents the identifier of an endpoint. In  LCS, this is the LCS address
  // encoded in a JID like format.  In XMPP, it is empty.
  std::string endpoint_id;

  Channel channel;

  inline const std::string& id() const {
    return (channel == Channel::LCS) ? endpoint_id : jid;
  }

  inline bool empty() const { return jid.empty(); }

  bool operator==(const SignalingAddress& other);
  bool operator!=(const SignalingAddress& other);
};

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
    EXPIRED,
    GENERAL_ERROR,
    FAILED_APPLICATION,
    INCOMPATIBLE_PARAMETERS,
  };


  JingleMessage();
  JingleMessage(const SignalingAddress& to,
                ActionType action_value,
                const std::string& sid_value);
  ~JingleMessage();

  // Caller keeps ownership of |stanza|.
  static bool IsJingleMessage(const buzz::XmlElement* stanza);
  static std::string GetActionName(ActionType action);

  // Caller keeps ownership of |stanza|. |error| is set to debug error
  // message when parsing fails.
  bool ParseXml(const buzz::XmlElement* stanza, std::string* error);

  std::unique_ptr<buzz::XmlElement> ToXml() const;

  SignalingAddress from;
  SignalingAddress to;
  ActionType action = UNKNOWN_ACTION;
  std::string sid;

  std::string initiator;

  std::unique_ptr<ContentDescription> description;

  std::unique_ptr<buzz::XmlElement> transport_info;

  // Content of session-info messages.
  std::unique_ptr<buzz::XmlElement> info;

  // Value from the <reason> tag if it is present in the
  // message. Useful mainly for session-terminate messages, but Jingle
  // spec allows it in any message.
  Reason reason = UNKNOWN_REASON;

  // Value from the <google:remoting:error-code> tag if it is present in the
  // message. Useful mainly for session-terminate messages. If it's UNKNOWN,
  // or reason is UNKNOWN_REASON, this field will be ignored in the xml output.
  ErrorCode error_code = UNKNOWN_ERROR;
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
  std::unique_ptr<buzz::XmlElement> ToXml(
      const buzz::XmlElement* request_stanza) const;

  ReplyType type;
  ErrorType error_type;
  std::string text;
};

struct IceTransportInfo {
  IceTransportInfo();
  ~IceTransportInfo();
  struct NamedCandidate {
    NamedCandidate() = default;
    NamedCandidate(const std::string& name,
                   const cricket::Candidate& candidate);

    std::string name;
    cricket::Candidate candidate;
  };

  struct IceCredentials {
    IceCredentials() = default;
    IceCredentials(std::string channel,
                   std::string ufrag,
                   std::string password);

    std::string channel;
    std::string ufrag;
    std::string password;
  };

  // Caller keeps ownership of |stanza|. |error| is set to debug error
  // message when parsing fails.
  bool ParseXml(const buzz::XmlElement* stanza);
  std::unique_ptr<buzz::XmlElement> ToXml() const;

  std::list<IceCredentials> ice_credentials;
  std::list<NamedCandidate> candidates;
};

}  // protocol
}  // remoting

#endif  // REMOTING_PROTOCOL_JINGLE_MESSAGES_H_
