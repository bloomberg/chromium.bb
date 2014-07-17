// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_H_
#define REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
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

const char kElevatingSwitchName[] = "elevate";
const char kInputSwitchName[] = "input";
const char kOutputSwitchName[] = "output";

namespace protocol {
class PairingRegistry;
}  // namespace protocol

// Implementation of the me2me native messaging host.
class Me2MeNativeMessagingHost {
 public:
  typedef NativeMessagingChannel::SendMessageCallback SendMessageCallback;

  Me2MeNativeMessagingHost(
      bool needs_elevation,
      intptr_t parent_window_handle,
      scoped_ptr<NativeMessagingChannel> channel,
      scoped_refptr<DaemonController> daemon_controller,
      scoped_refptr<protocol::PairingRegistry> pairing_registry,
      scoped_ptr<OAuthClient> oauth_client);
  virtual ~Me2MeNativeMessagingHost();

  void Start(const base::Closure& quit_closure);

 private:
  // Callback to process messages from the native messaging client through
  // |channel_|.
  void ProcessRequest(scoped_ptr<base::DictionaryValue> message);

  // These "Process.." methods handle specific request types. The |response|
  // dictionary is pre-filled by ProcessMessage() with the parts of the
  // response already known ("id" and "type" fields).
  void ProcessHello(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessClearPairedClients(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessDeletePairedClient(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessGetHostName(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessGetPinHash(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessGenerateKeyPair(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessUpdateDaemonConfig(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessGetDaemonConfig(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessGetPairedClients(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessGetUsageStatsConsent(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessStartDaemon(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessStopDaemon(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessGetDaemonState(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessGetHostClientId(
      scoped_ptr<base::DictionaryValue> message,
      scoped_ptr<base::DictionaryValue> response);
  void ProcessGetCredentialsFromAuthCode(
      scoped_ptr<base::DictionaryValue> message,
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

  void OnError();

  void Stop();

  // Returns true if the request was successfully delegated to the elevated
  // host and false otherwise.
  bool DelegateToElevatedHost(scoped_ptr<base::DictionaryValue> message);

#if defined(OS_WIN)
  // Create and connect to an elevated host process if necessary.
  // |elevated_channel_| will contain the native messaging channel to the
  // elevated host if the function succeeds.
  void Me2MeNativeMessagingHost::EnsureElevatedHostCreated();

  // Callback to process messages from the elevated host through
  // |elevated_channel_|.
  void ProcessDelegateResponse(scoped_ptr<base::DictionaryValue> message);

  // Disconnect and shut down the elevated host.
  void DisconnectElevatedHost();

  // Native messaging channel used to communicate with the elevated host.
  scoped_ptr<NativeMessagingChannel> elevated_channel_;

  // Timer to control the lifetime of the elevated host.
  base::OneShotTimer<Me2MeNativeMessagingHost> elevated_host_timer_;
#endif  // defined(OS_WIN)

  bool needs_elevation_;

  // Handle of the parent window.
  intptr_t parent_window_handle_;

  base::Closure quit_closure_;

  // Native messaging channel used to communicate with the native message
  // client.
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

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_H_
