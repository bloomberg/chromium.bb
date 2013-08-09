// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_NATIVE_MESSAGING_HOST_H_
#define REMOTING_HOST_SETUP_NATIVE_MESSAGING_HOST_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "remoting/host/setup/daemon_controller.h"
#include "remoting/host/setup/native_messaging_reader.h"
#include "remoting/host/setup/native_messaging_writer.h"

namespace base {
class DictionaryValue;
class ListValue;
class SingleThreadTaskRunner;
class Value;
}  // namespace base

namespace remoting {

namespace protocol {
class PairingRegistry;
}  // namespace protocol

// Implementation of the native messaging host process.
class NativeMessagingHost {
 public:
  NativeMessagingHost(
      scoped_ptr<DaemonController> daemon_controller,
      scoped_refptr<protocol::PairingRegistry> pairing_registry,
      base::PlatformFile input,
      base::PlatformFile output,
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      const base::Closure& quit_closure);
  ~NativeMessagingHost();

  // Starts reading and processing messages.
  void Start();

  // Posts |quit_closure| to |caller_task_runner|. This gets called whenever an
  // error is encountered during reading and processing messages.
  void Shutdown();

 private:
  // Processes a message received from the client app.
  void ProcessMessage(scoped_ptr<base::Value> message);

  // These "Process.." methods handle specific request types. The |response|
  // dictionary is pre-filled by ProcessMessage() with the parts of the
  // response already known ("id" and "type" fields).
  bool ProcessHello(const base::DictionaryValue& message,
                    scoped_ptr<base::DictionaryValue> response);
  bool ProcessClearPairedClients(const base::DictionaryValue& message,
                                 scoped_ptr<base::DictionaryValue> response);
  bool ProcessDeletePairedClient(const base::DictionaryValue& message,
                                 scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetHostName(const base::DictionaryValue& message,
                          scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetPinHash(const base::DictionaryValue& message,
                         scoped_ptr<base::DictionaryValue> response);
  bool ProcessGenerateKeyPair(const base::DictionaryValue& message,
                              scoped_ptr<base::DictionaryValue> response);
  bool ProcessUpdateDaemonConfig(const base::DictionaryValue& message,
                                 scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetDaemonConfig(const base::DictionaryValue& message,
                              scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetPairedClients(const base::DictionaryValue& message,
                               scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetUsageStatsConsent(const base::DictionaryValue& message,
                                   scoped_ptr<base::DictionaryValue> response);
  bool ProcessStartDaemon(const base::DictionaryValue& message,
                          scoped_ptr<base::DictionaryValue> response);
  bool ProcessStopDaemon(const base::DictionaryValue& message,
                         scoped_ptr<base::DictionaryValue> response);
  bool ProcessGetDaemonState(const base::DictionaryValue& message,
                             scoped_ptr<base::DictionaryValue> response);

  // Sends a response back to the client app. This can be called on either the
  // main message loop or the DaemonController's internal thread, so it
  // PostTask()s to the main thread if necessary.
  void SendResponse(scoped_ptr<base::DictionaryValue> response);

  // These Send... methods get called on the DaemonController's internal thread,
  // or on the calling thread if called by the PairingRegistry.
  // These methods fill in the |response| dictionary from the other parameters,
  // and pass it to SendResponse().
  void SendConfigResponse(scoped_ptr<base::DictionaryValue> response,
                          scoped_ptr<base::DictionaryValue> config);
  void SendPairedClientsResponse(scoped_ptr<base::DictionaryValue> response,
                                 scoped_ptr<base::ListValue> pairings);
  void SendUsageStatsConsentResponse(scoped_ptr<base::DictionaryValue> response,
                                     bool supported,
                                     bool allowed,
                                     bool set_by_policy);
  void SendAsyncResult(scoped_ptr<base::DictionaryValue> response,
                       DaemonController::AsyncResult result);
  void SendBooleanResult(scoped_ptr<base::DictionaryValue> response,
                         bool result);

  // Callbacks may be invoked by e.g. DaemonController during destruction,
  // which use |weak_ptr_|, so it's important that it be the last member to be
  // destroyed.
  base::WeakPtr<NativeMessagingHost> weak_ptr_;

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  base::Closure quit_closure_;

  NativeMessagingReader native_messaging_reader_;
  NativeMessagingWriter native_messaging_writer_;

  // The DaemonController may post tasks to this object during destruction (but
  // not afterwards), so it needs to be destroyed before other members of this
  // class (except for |weak_factory_|).
  scoped_ptr<remoting::DaemonController> daemon_controller_;

  // Used to load and update the paired clients for this host.
  scoped_refptr<protocol::PairingRegistry> pairing_registry_;

  base::WeakPtrFactory<NativeMessagingHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeMessagingHost);
};

// Creates a NativeMessagingHost instance, attaches it to stdin/stdout and runs
// the message loop until NativeMessagingHost signals shutdown.
int NativeMessagingHostMain();

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_NATIVE_MESSAGING_HOST_H_
