// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_CAST_SOCKET_H_
#define CAST_COMMON_CHANNEL_CAST_SOCKET_H_

#include <vector>

#include "platform/api/tls_connection.h"

namespace cast {
namespace channel {

using openscreen::Error;
using TlsConnection = openscreen::platform::TlsConnection;

class CastMessage;

uint32_t GetNextSocketId();

// Represents a simple message-oriented socket for communicating with the Cast
// V2 protocol.  It isn't thread-safe, so it should only be used on the same
// TaskRunner thread as its TlsConnection.
class CastSocket : public TlsConnection::Client {
 public:
  class Client {
   public:
    virtual ~Client() = default;

    // Called when a terminal error on |socket| has occurred.
    virtual void OnError(CastSocket* socket, Error error) = 0;

    virtual void OnMessage(CastSocket* socket, CastMessage message) = 0;
  };

  CastSocket(std::unique_ptr<TlsConnection> connection,
             Client* client,
             uint32_t socket_id);
  ~CastSocket();

  // Sends |message| immediately unless the underlying TLS connection is
  // write-blocked, in which case |message| will be queued.  An error will be
  // returned if |message| cannot be serialized for any reason, even while
  // write-blocked.
  Error SendMessage(const CastMessage& message);

  void SetClient(Client* client);

  uint32_t socket_id() const { return socket_id_; }

  // TlsConnection::Client overrides.
  void OnWriteBlocked(TlsConnection* connection) override;
  void OnWriteUnblocked(TlsConnection* connection) override;
  void OnError(TlsConnection* connection, Error error) override;
  void OnRead(TlsConnection* connection, std::vector<uint8_t> block) override;

 private:
  enum class State {
    kOpen,
    kBlocked,
    kError,
  };

  Client* client_;  // May never be null.
  const std::unique_ptr<TlsConnection> connection_;
  std::vector<uint8_t> read_buffer_;
  const uint32_t socket_id_;
  State state_ = State::kOpen;
  std::vector<std::vector<uint8_t>> message_queue_;
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_COMMON_CHANNEL_CAST_SOCKET_H_
