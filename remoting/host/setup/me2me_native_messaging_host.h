// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_H_
#define REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/host/native_messaging/native_messaging_channel.h"
#include "remoting/host/setup/daemon_controller.h"
#include "remoting/host/setup/oauth_client.h"

namespace base {
class DictionaryValue;
class ListValue;
}  // namespace base

namespace gaia {
class GaiaOAuthClient;
}  // namespace gaia

namespace remoting {

namespace protocol {
class PairingRegistry;
}  // namespace protocol

// Implementation of the native messaging host process.
class NativeMessagingHost : public NativeMessagingChannel::Delegate {
 public:
  typedef NativeMessagingChannel::SendResponseCallback SendResponseCallback;

  NativeMessagingHost(
      scoped_refptr<DaemonController> daemon_controller,
      scoped_refptr<protocol::PairingRegistry> pairing_registry,
      scoped_ptr<OAuthClient> oauth_client);
  virtual ~NativeMessagingHost();

  // NativeMessagingChannel::Delegate interface.
  virtual void ProcessMessage(scoped_ptr<base::DictionaryValue> message,
                              const SendResponseCallback& done) OVERRIDE;

 private:
  // These "Process.." methods handle specific request types. The |response|
  // dictionary is pre-filled by ProcessMessage() with the parts of the
  // response already known ("id" and "type" fields).
  bool ProcessHello(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessClearPairedClients(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessDeletePairedClient(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessGetHostName(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessGetPinHash(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessGenerateKeyPair(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessUpdateDaemonConfig(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessGetDaemonConfig(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessGetPairedClients(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessGetUsageStatsConsent(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessStartDaemon(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessStopDaemon(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessGetDaemonState(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessGetHostClientId(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);
  bool ProcessGetCredentialsFromAuthCode(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response,
      const SendResponseCallback& done);

  // These Send... methods get called on the DaemonController's internal thread,
  // or on the calling thread if called by the PairingRegistry.
  // These methods fill in the |response| dictionary from the other parameters,
  // and pass it to SendResponse().
  void SendConfigResponse(const SendResponseCallback& done,
                          scoped_ptr<base::DictionaryValue> response,
                          scoped_ptr<base::DictionaryValue> config);
  void SendPairedClientsResponse(const SendResponseCallback& done,
                                 scoped_ptr<base::DictionaryValue> response,
                                 scoped_ptr<base::ListValue> pairings);
  void SendUsageStatsConsentResponse(
      const SendResponseCallback& done,
      scoped_ptr<base::DictionaryValue> response,
      const DaemonController::UsageStatsConsent& consent);
  void SendAsyncResult(const SendResponseCallback& done,
                       scoped_ptr<base::DictionaryValue> response,
                       DaemonController::AsyncResult result);
  void SendBooleanResult(const SendResponseCallback& done,
                         scoped_ptr<base::DictionaryValue> response,
                         bool result);
  void SendCredentialsResponse(const SendResponseCallback& done,
                               scoped_ptr<base::DictionaryValue> response,
                               const std::string& user_email,
                               const std::string& refresh_token);

  scoped_refptr<DaemonController> daemon_controller_;

  // Used to load and update the paired clients for this host.
  scoped_refptr<protocol::PairingRegistry> pairing_registry_;

  // Used to exchange the service account authorization code for credentials.
  scoped_ptr<OAuthClient> oauth_client_;

  base::ThreadChecker thread_checker_;

  base::WeakPtr<NativeMessagingHost> weak_ptr_;
  base::WeakPtrFactory<NativeMessagingHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeMessagingHost);
};

// Creates a NativeMessagingHost instance, attaches it to stdin/stdout and runs
// the message loop until NativeMessagingHost signals shutdown.
int NativeMessagingHostMain();

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_H_
