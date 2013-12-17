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

// Implementation of the me2me native messaging host.
class Me2MeNativeMessagingHost {
 public:
  typedef NativeMessagingChannel::SendMessageCallback SendMessageCallback;

  Me2MeNativeMessagingHost(
      scoped_ptr<NativeMessagingChannel> channel,
      scoped_refptr<DaemonController> daemon_controller,
      scoped_refptr<protocol::PairingRegistry> pairing_registry,
      scoped_ptr<OAuthClient> oauth_client);
  virtual ~Me2MeNativeMessagingHost();

  void Start(const base::Closure& quit_closure);

 private:
  void ProcessMessage(scoped_ptr<base::DictionaryValue> message);

  // These "Process.." methods handle specific request types. The |response|
  // dictionary is pre-filled by ProcessMessage() with the parts of the
  // response already known ("id" and "type" fields).
  bool ProcessHello(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessClearPairedClients(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessDeletePairedClient(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetHostName(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetPinHash(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessGenerateKeyPair(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessUpdateDaemonConfig(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetDaemonConfig(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetPairedClients(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetUsageStatsConsent(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessStartDaemon(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessStopDaemon(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetDaemonState(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetHostClientId(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetCredentialsFromAuthCode(
      const base::DictionaryValue& message,
      scoped_ptr<base::DictionaryValue> response);

  // These Send... methods get called on the DaemonController's internal thread,
  // or on the calling thread if called by the PairingRegistry.
  // These methods fill in the |response| dictionary from the other parameters,
  // and pass it to SendResponse().
  void SendConfigResponse(scoped_ptr<base::DictionaryValue> response,
                          scoped_ptr<base::DictionaryValue> config);
  void SendPairedClientsResponse(scoped_ptr<base::DictionaryValue> response,
                                 scoped_ptr<base::ListValue> pairings);
  void SendUsageStatsConsentResponse(
      scoped_ptr<base::DictionaryValue> response,
      const DaemonController::UsageStatsConsent& consent);
  void SendAsyncResult(scoped_ptr<base::DictionaryValue> response,
                       DaemonController::AsyncResult result);
  void SendBooleanResult(scoped_ptr<base::DictionaryValue> response,
                         bool result);
  void SendCredentialsResponse(scoped_ptr<base::DictionaryValue> response,
                               const std::string& user_email,
                               const std::string& refresh_token);

  scoped_ptr<NativeMessagingChannel> channel_;
  scoped_refptr<DaemonController> daemon_controller_;

  // Used to load and update the paired clients for this host.
  scoped_refptr<protocol::PairingRegistry> pairing_registry_;

  // Used to exchange the service account authorization code for credentials.
  scoped_ptr<OAuthClient> oauth_client_;

  base::ThreadChecker thread_checker_;

  base::WeakPtr<Me2MeNativeMessagingHost> weak_ptr_;
  base::WeakPtrFactory<Me2MeNativeMessagingHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Me2MeNativeMessagingHost);
};

// Creates a Me2MeNativeMessagingHost instance, attaches it to stdin/stdout and
// runs the message loop until Me2MeNativeMessagingHost signals shutdown.
int Me2MeNativeMessagingHostMain();

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_H_
