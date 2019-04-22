// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_ATTEMPT_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_ATTEMPT_H_

#include "base/macros.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace secure_channel {

// A handle for clients to own while waiting for a connection to establish (or
// fail); it is returned by SecureChannelClient's InitiateConnectionToDevice()
// or ListenForConnectionFromDevice() method. Clients should implement the
// ConnectionAttempt::Delegate interface, and call AddDelegate() on the object
// immediately after receiving it. To cancel a connection attempt, simply delete
// the object. After receiving the OnConnection() callback, it is fine to delete
// the ConnectionAttempt object; the returned ClientChannel object will
// be the client's way to interface with the API moving forward.
class ConnectionAttempt {
 public:
  class Delegate {
   public:
    virtual ~Delegate();
    virtual void OnConnectionAttemptFailure(
        mojom::ConnectionAttemptFailureReason reason) = 0;
    virtual void OnConnection(std::unique_ptr<ClientChannel> channel) = 0;
  };

  ConnectionAttempt();
  virtual ~ConnectionAttempt();

  void SetDelegate(Delegate* delegate);

 protected:
  void NotifyConnectionAttemptFailure(
      mojom::ConnectionAttemptFailureReason reason);
  void NotifyConnection(std::unique_ptr<ClientChannel> channel);

 private:
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionAttempt);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_ATTEMPT_H_
