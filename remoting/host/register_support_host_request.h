// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REGISTER_SUPPORT_HOST_REQUEST_H_
#define REMOTING_HOST_REGISTER_SUPPORT_HOST_REQUEST_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/signaling/signal_strategy.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace base {
class TimeDelta;
}  // namespace base

namespace remoting {

class IqRequest;
class IqSender;

// RegisterSupportHostRequest sends a request to register the host for
// a SupportID, as soon as the associated SignalStrategy becomes
// connected. When a response is received from the bot, it calls the
// callback specified in the Init() method.
class RegisterSupportHostRequest : public SignalStrategy::Listener {
 public:
  // First parameter is set to true on success. Second parameter is
  // the new SessionID received from the bot. Third parameter is the
  // amount of time until that id expires.
  typedef base::Callback<void(bool, const std::string&,
                              const base::TimeDelta&)> RegisterCallback;

  // |signal_strategy| and |key_pair| must outlive this
  // object. |callback| is called when registration response is
  // received from the server. Callback is never called if the bot
  // malfunctions and doesn't respond to the request.
  //
  // TODO(sergeyu): This class should have timeout for the bot
  // response.
  RegisterSupportHostRequest(SignalStrategy* signal_strategy,
                             scoped_refptr<RsaKeyPair> key_pair,
                             const std::string& directory_bot_jid,
                             const RegisterCallback& callback);
  virtual ~RegisterSupportHostRequest();

  // HostStatusObserver implementation.
  virtual void OnSignalStrategyStateChange(
      SignalStrategy::State state) OVERRIDE;
  virtual bool OnSignalStrategyIncomingStanza(
      const buzz::XmlElement* stanza) OVERRIDE;

 private:
  void DoSend();

  scoped_ptr<buzz::XmlElement> CreateRegistrationRequest(
      const std::string& jid);
  scoped_ptr<buzz::XmlElement> CreateSignature(const std::string& jid);

  void ProcessResponse(IqRequest* request, const buzz::XmlElement* response);
  bool ParseResponse(const buzz::XmlElement* response,
                     std::string* support_id, base::TimeDelta* lifetime);

  void CallCallback(
      bool success, const std::string& support_id, base::TimeDelta lifetime);

  SignalStrategy* signal_strategy_;
  scoped_refptr<RsaKeyPair> key_pair_;
  std::string directory_bot_jid_;
  RegisterCallback callback_;

  scoped_ptr<IqSender> iq_sender_;
  scoped_ptr<IqRequest> request_;

  DISALLOW_COPY_AND_ASSIGN(RegisterSupportHostRequest);
};

}  // namespace remoting

#endif  // REMOTING_HOST_REGISTER_SUPPORT_HOST_REQUEST_H_
