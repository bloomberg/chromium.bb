// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HEARTBEAT_SENDER_H_
#define REMOTING_HOST_HEARTBEAT_SENDER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/signaling/signal_strategy.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {

class RsaKeyPair;
class IqRequest;
class IqSender;

// HeartbeatSender periodically sends heartbeat stanzas to the Chromoting Bot.
// Each heartbeat stanza looks as follows:
//
// <iq type="set" to="remoting@bot.talk.google.com"
//     from="user@gmail.com/chromoting123123" id="5" xmlns="jabber:client">
//   <rem:heartbeat rem:hostid="a1ddb11e-8aef-11df-bccf-18a905b9cb5a"
//                  rem:sequence-id="456"
//                  xmlns:rem="google:remoting">
//     <rem:signature>.signature.</rem:signature>
//   </rem:heartbeat>
// </iq>
//
// The sequence-id attribute of the heartbeat is a zero-based incrementally
// increasing integer unique to each heartbeat from a single host.
// The Bot checks the value, and if it is incorrect, includes the
// correct value in the result stanza. The host should then send another
// heartbeat, with the correct sequence-id, and increment the sequence-id in
// susbequent heartbeats.
// The signature is a base-64 encoded SHA-1 hash, signed with the host's
// private RSA key. The message being signed is the full Jid concatenated with
// the sequence-id, separated by one space. For example, for the heartbeat
// stanza above, the message that is signed is
// "user@gmail.com/chromoting123123 456".
//
// The Bot sends the following result stanza in response to each successful
// heartbeat:
//
//  <iq type="set" from="remoting@bot.talk.google.com"
//      to="user@gmail.com/chromoting123123" id="5" xmlns="jabber:client">
//    <rem:heartbeat-result xmlns:rem="google:remoting">
//      <rem:set-interval>300</rem:set-interval>
//    </rem:heartbeat-result>
//  </iq>
//
// The set-interval tag is used to specify desired heartbeat interval
// in seconds. The heartbeat-result and the set-interval tags are
// optional. Host uses default heartbeat interval if it doesn't find
// set-interval tag in the result Iq stanza it receives from the
// server.
// If the heartbeat's sequence-id was incorrect, the Bot sends a result
// stanza of this form:
//
//  <iq type="set" from="remoting@bot.talk.google.com"
//      to="user@gmail.com/chromoting123123" id="5" xmlns="jabber:client">
//    <rem:heartbeat-result xmlns:rem="google:remoting">
//      <rem:expected-sequence-id>654</rem:expected-sequence-id>
//    </rem:heartbeat-result>
//  </iq>
class HeartbeatSender : public SignalStrategy::Listener {
 public:
  class Listener {
   public:
    virtual ~Listener() { }

    // Invoked after the first successful heartbeat.
    virtual void OnHeartbeatSuccessful() = 0;

    // Invoked when the host ID is permanently not recognized by the server.
    virtual void OnUnknownHostIdError() = 0;
  };

  // |signal_strategy| and |delegate| must outlive this
  // object. Heartbeats will start when the supplied SignalStrategy
  // enters the CONNECTED state.
  HeartbeatSender(Listener* listener,
                  const std::string& host_id,
                  SignalStrategy* signal_strategy,
                  scoped_refptr<RsaKeyPair> key_pair,
                  const std::string& directory_bot_jid);
  virtual ~HeartbeatSender();

  // SignalStrategy::Listener interface.
  virtual void OnSignalStrategyStateChange(
      SignalStrategy::State state) OVERRIDE;
  virtual bool OnSignalStrategyIncomingStanza(
      const buzz::XmlElement* stanza) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(HeartbeatSenderTest, DoSendStanza);
  FRIEND_TEST_ALL_PREFIXES(HeartbeatSenderTest,
                           DoSendStanzaWithExpectedSequenceId);
  FRIEND_TEST_ALL_PREFIXES(HeartbeatSenderTest, CreateHeartbeatMessage);
  FRIEND_TEST_ALL_PREFIXES(HeartbeatSenderTest, ProcessResponseSetInterval);
  FRIEND_TEST_ALL_PREFIXES(HeartbeatSenderTest,
                           ProcessResponseExpectedSequenceId);

  void SendStanza();
  void ResendStanza();
  void DoSendStanza();
  void ProcessResponse(IqRequest* request, const buzz::XmlElement* response);
  void SetInterval(int interval);
  void SetSequenceId(int sequence_id);

  // Helper methods used by DoSendStanza() to generate heartbeat stanzas.
  scoped_ptr<buzz::XmlElement> CreateHeartbeatMessage();
  scoped_ptr<buzz::XmlElement> CreateSignature();

  Listener* listener_;
  std::string host_id_;
  SignalStrategy* signal_strategy_;
  scoped_refptr<RsaKeyPair> key_pair_;
  std::string directory_bot_jid_;
  scoped_ptr<IqSender> iq_sender_;
  scoped_ptr<IqRequest> request_;
  int interval_ms_;
  base::RepeatingTimer<HeartbeatSender> timer_;
  base::OneShotTimer<HeartbeatSender> timer_resend_;
  int sequence_id_;
  bool sequence_id_was_set_;
  int sequence_id_recent_set_num_;
  bool heartbeat_succeeded_;
  int failed_startup_heartbeat_count_;

  DISALLOW_COPY_AND_ASSIGN(HeartbeatSender);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HEARTBEAT_SENDER_H_
