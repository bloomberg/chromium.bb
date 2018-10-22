// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_MESSENGER_IMPL_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_MESSENGER_IMPL_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/components/proximity_auth/messenger.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"
#include "components/cryptauth/connection.h"
#include "components/cryptauth/connection_observer.h"

namespace base {
class DictionaryValue;
}

namespace cryptauth {
class SecureContext;
}

namespace proximity_auth {

// Concrete implementation of the Messenger interface.
class MessengerImpl : public Messenger,
                      public cryptauth::ConnectionObserver,
                      public chromeos::secure_channel::ClientChannel::Observer {
 public:
  // Constructs a messenger that sends and receives messages.
  //
  // If the |chromeos::features::kMultiDeviceApi| flag is enabled, messages are
  // relayed over the provided |channel|, and |connection| and |secure_context|
  // are ignored.
  //
  // If not, messages are relayed over |connection|, using the |secure_context|
  // to encrypt and decrypt the messages. |channel| is ignored.
  //
  // The messenger begins observing messages as soon as it is constructed.
  MessengerImpl(
      std::unique_ptr<cryptauth::Connection> connection,
      std::unique_ptr<cryptauth::SecureContext> secure_context,
      std::unique_ptr<chromeos::secure_channel::ClientChannel> channel);
  ~MessengerImpl() override;

  // Messenger:
  void AddObserver(MessengerObserver* observer) override;
  void RemoveObserver(MessengerObserver* observer) override;
  bool SupportsSignIn() const override;
  void DispatchUnlockEvent() override;
  void RequestDecryption(const std::string& challenge) override;
  void RequestUnlock() override;
  cryptauth::SecureContext* GetSecureContext() const override;
  cryptauth::Connection* GetConnection() const override;
  chromeos::secure_channel::ClientChannel* GetChannel() const override;

  // Exposed for testing.
  cryptauth::Connection* connection() { return connection_.get(); }

 private:
  // Internal data structure to represent a pending message that either hasn't
  // been sent yet or is waiting for a response from the remote device.
  struct PendingMessage {
    PendingMessage();
    explicit PendingMessage(const base::DictionaryValue& message);
    explicit PendingMessage(const std::string& message);
    ~PendingMessage();

    // The message, serialized as JSON.
    const std::string json_message;

    // The message type. This is possible to parse from the |json_message|; it's
    // stored redundantly for convenience.
    const std::string type;
  };

  // Pops the first of the |queued_messages_| and sends it to the remote device.
  void ProcessMessageQueue();

  // Called when the message is encoded so it can be sent over the connection.
  void OnMessageEncoded(const std::string& encoded_message);

  // Handles an incoming "status_update" |message|, parsing and notifying
  // observers of the content.
  void HandleRemoteStatusUpdateMessage(const base::DictionaryValue& message);

  // Handles an incoming "decrypt_response" message, parsing and notifying
  // observers of the decrypted content.
  void HandleDecryptResponseMessage(const base::DictionaryValue& message);

  // Handles an incoming "unlock_response" message, notifying observers of the
  // response.
  void HandleUnlockResponseMessage(const base::DictionaryValue& message);

  // cryptauth::ConnectionObserver:
  void OnConnectionStatusChanged(
      cryptauth::Connection* connection,
      cryptauth::Connection::Status old_status,
      cryptauth::Connection::Status new_status) override;
  void OnMessageReceived(const cryptauth::Connection& connection,
                         const cryptauth::WireMessage& wire_message) override;
  void OnSendCompleted(const cryptauth::Connection& connection,
                       const cryptauth::WireMessage& wire_message,
                       bool success) override;

  // chromeos::secure_channel::ClientChannel::Observer:
  void OnDisconnected() override;
  void OnMessageReceived(const std::string& payload) override;

  // Called when a message has been recevied from the remote device. The message
  // should be a valid JSON string.
  void HandleMessage(const std::string& message);

  // Called when a message has been sent to the remote device.
  void OnSendMessageResult(bool success);

  // The connection used to send and receive events and status updates.
  std::unique_ptr<cryptauth::Connection> connection_;

  // Used to encrypt and decrypt payloads sent and received over the
  // |connection_|.
  std::unique_ptr<cryptauth::SecureContext> secure_context_;

  // Authenticated end-to-end channel used to communicate with the remote
  // device.
  std::unique_ptr<chromeos::secure_channel::ClientChannel> channel_;

  // The registered observers of |this_| messenger.
  base::ObserverList<MessengerObserver>::Unchecked observers_;

  // Queue of messages to send to the remote device.
  base::circular_deque<PendingMessage> queued_messages_;

  // The current message being sent or waiting on the remote device for a
  // response. Null if there is no message currently in this state.
  std::unique_ptr<PendingMessage> pending_message_;

  base::WeakPtrFactory<MessengerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MessengerImpl);
};

}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_MESSENGER_IMPL_H_
