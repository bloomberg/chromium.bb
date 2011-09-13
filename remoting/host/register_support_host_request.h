// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SUPPORT_HOST_REGISTER_QUERY_H_
#define REMOTING_HOST_SUPPORT_HOST_REGISTER_QUERY_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/host_status_observer.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class MessageLoop;

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace base {
class TimeDelta;
}  // namespace base

namespace remoting {

class IqRequest;
class MutableHostConfig;

// RegisterSupportHostRequest sends support host registeration request
// to the Chromoting Bot. It listens to the status of the host using
// HostStatusObserver interface and sends the request when signalling
// channel is connected. When a response is received from the bot, it
// calls the callback specified in the Init() method.
class RegisterSupportHostRequest : public HostStatusObserver {
 public:
  // First parameter is set to true on success. Second parameter is
  // the new SessionID received from the bot. Third parameter is the
  // amount of time until that id expires.
  typedef base::Callback<void(bool, const std::string&,
                              const base::TimeDelta&)> RegisterCallback;

  RegisterSupportHostRequest();
  virtual ~RegisterSupportHostRequest();

  // Provide the configuration to use to register the host, and a
  // callback to invoke when a registration response is received.
  // |callback| is called when registration response is received from
  // the server. Ownership of |callback| is given to the request
  // object. Caller must ensure that the callback object is valid
  // while signalling connection exists. Returns false on falure
  // (e.g. config is invalid). Callback is never called if the bot
  // malfunctions and doesn't respond to the request.
  bool Init(HostConfig* config, const RegisterCallback& callback);

  // HostStatusObserver implementation.
  virtual void OnSignallingConnected(SignalStrategy* signal_strategy,
                                     const std::string& full_jid) OVERRIDE;
  virtual void OnSignallingDisconnected() OVERRIDE;
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnAccessDenied() OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

 private:
  void DoSend();

  // Caller owns the result.
  buzz::XmlElement* CreateRegistrationRequest(const std::string& jid);
  buzz::XmlElement* CreateSignature(const std::string& jid);

  void ProcessResponse(const buzz::XmlElement* response);
  bool ParseResponse(const buzz::XmlElement* response,
                     std::string* support_id, base::TimeDelta* lifetime);

  MessageLoop* message_loop_;
  RegisterCallback callback_;
  scoped_ptr<IqRequest> request_;
  HostKeyPair key_pair_;

  DISALLOW_COPY_AND_ASSIGN(RegisterSupportHostRequest);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SUPPORT_HOST_REGISTER_QUERY_H_
