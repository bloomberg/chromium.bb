// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_STATUS_SENDER_H_
#define REMOTING_HOST_STATUS_SENDER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/signaling/signal_strategy.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {

class RsaKeyPair;
class IqSender;

// HostStatusSender sends the host status to the Chromoting Bot.
// Each host status stanza looks as follows:
//
// <cli:iq type="set" to="user@gmail.com/chromoting123123"
//     id="123" xmlns:cli="jabber:client">
//   <rem:host-status rem:hostid="0"
//       rem:status="OFFLINE" rem:exit-code="INVALID_HOST_CONFIGURATION"
//       xmlns:rem="google:remoting">
//     <rem:signature rem:time="1372878097">.signature.</rem:signature>
//     <rem:host-version>30.0.1554.0</rem:host-version>
//     <rem:log><rem:entry cpu="x86_64" event-name="host-status"
//         host-version="30.0.1560.0" os-name="Linux" role="host"/>
//     </rem:log>
//   </rem:host-status>
// </cli:iq>
//
// The signature is a base-64 encoded SHA-1 hash, signed with the host's
// private RSA key. The message being signed contains the full Jid (e.g.
// "user@gmail.com/chromoting123123"), the timestamp, the host status,
// and the exit code.
class HostStatusSender : SignalStrategy::Listener {
 public:

  enum HostStatus {
    OFFLINE = 0,
    ONLINE = 1
  };

  HostStatusSender(const std::string& host_id,
                   SignalStrategy* signal_strategy,
                   scoped_refptr<RsaKeyPair> key_pair,
                   const std::string& directory_bot_jid);
  virtual ~HostStatusSender();

  // SignalStrategy::Listener interface.
  virtual void OnSignalStrategyStateChange(
      SignalStrategy::State state) OVERRIDE;
  virtual bool OnSignalStrategyIncomingStanza(
      const buzz::XmlElement* stanza) OVERRIDE;

  // APIs for sending host status XMPP messages to the chromoting bot.
  // status: the reason (exit code) why the host is offline.
  void SendOnlineStatus();
  void SendOfflineStatus(HostExitCodes exit_code);

  inline static const char* HostStatusToString(HostStatus host_status) {
    return host_status_strings_[host_status];
  }

 private:
  // Helper method for sending either an online or an offline status message.
  void SendHostStatus(HostStatus status, HostExitCodes exit_code);

  // Helper method to compose host status stanzas.
  scoped_ptr<buzz::XmlElement> CreateHostStatusMessage(
      HostStatus status, HostExitCodes exit_code);

  // Helper method to create the signature blob used in the host status stanza.
  scoped_ptr<buzz::XmlElement> CreateSignature(
      HostStatus status, HostExitCodes exit_code);

  std::string host_id_;
  SignalStrategy* signal_strategy_;
  scoped_refptr<RsaKeyPair> key_pair_;
  std::string directory_bot_jid_;
  scoped_ptr<IqSender> iq_sender_;

  // The string representation of the HostStatus values.
  static const char* const host_status_strings_[2];

  DISALLOW_COPY_AND_ASSIGN(HostStatusSender);
};

}  // namespace remoting

#endif  // REMOTING_HOST_STATUS_SENDER_H_
