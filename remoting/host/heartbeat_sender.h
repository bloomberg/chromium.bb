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
#include "base/timer.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/jingle_glue/signal_strategy.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {

class HostKeyPair;
class IqRequest;
class IqSender;

// HeartbeatSender periodically sends heartbeat stanzas to the Chromoting Bot.
// Each heartbeat stanza looks as follows:
//
// <iq type="set" to="remoting@bot.talk.google.com"
//     from="user@gmail.com/chromoting123123" id="5" xmlns="jabber:client">
//   <rem:heartbeat rem:hostid="a1ddb11e-8aef-11df-bccf-18a905b9cb5a"
//                  xmlns:rem="google:remoting">
//     <rem:signature rem:time="1279061748">.signature.</rem:signature>
//   </rem:heartbeat>
// </iq>
//
// The time attribute of the signature is the decimal time when the message
// was sent in second since the epoch (01/01/1970). The signature is a BASE64
// encoded SHA-1/RSA signature created with the host's private key. The message
// being signed is the full Jid concatenated with the time value, separated by
// space. For example, for the heartbeat stanza above the message that is being
// signed is "user@gmail.com/chromoting123123 1279061748".
//
// Bot sends the following result stanza in response to each heartbeat:
//
//  <iq type="set" from="remoting@bot.talk.google.com"
//      to="user@gmail.com/chromoting123123" id="5" xmlns="jabber:client">
//    <rem:heartbeat-result xmlns:rem="google:remoting">
//      <rem:set-interval>300</rem:set-interval>
//    </rem:heartbeat>
//  </iq>
//
// The set-interval tag is used to specify desired heartbeat interval
// in seconds. The heartbeat-result and the set-interval tags are
// optional. Host uses default heartbeat interval if it doesn't find
// set-interval tag in the result Iq stanza it receives from the
// server.
class HeartbeatSender : public SignalStrategy::Listener {
 public:
  // Doesn't take ownership of |signal_strategy| or |key_pair|. Both
  // parameters must outlive this object. Heartbeats will start when
  // the supplied SignalStrategy enters the CONNECTED state.
  HeartbeatSender(const std::string& host_id,
                  SignalStrategy* signal_strategy,
                  HostKeyPair* key_pair);
  virtual ~HeartbeatSender();

  // SignalStrategy::Listener interface.
  virtual void OnSignalStrategyStateChange(
      SignalStrategy::State state) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(HeartbeatSenderTest, DoSendStanza);
  FRIEND_TEST_ALL_PREFIXES(HeartbeatSenderTest, CreateHeartbeatMessage);
  FRIEND_TEST_ALL_PREFIXES(HeartbeatSenderTest, ProcessResponse);

  void DoSendStanza();
  void ProcessResponse(const buzz::XmlElement* response);
  void SetInterval(int interval);

  // Helper methods used by DoSendStanza() to generate heartbeat stanzas.
  // Caller owns the result.
  buzz::XmlElement* CreateHeartbeatMessage();
  buzz::XmlElement* CreateSignature();

  std::string host_id_;
  SignalStrategy* signal_strategy_;
  HostKeyPair* key_pair_;
  scoped_ptr<IqSender> iq_sender_;
  scoped_ptr<IqRequest> request_;
  int interval_ms_;
  base::RepeatingTimer<HeartbeatSender> timer_;

  DISALLOW_COPY_AND_ASSIGN(HeartbeatSender);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HEARTBEAT_SENDER_H_
