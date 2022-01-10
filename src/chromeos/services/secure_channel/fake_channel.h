// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CHANNEL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CHANNEL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace secure_channel {

// Test mojom::Channel implementation.
class FakeChannel : public mojom::Channel {
 public:
  FakeChannel();

  FakeChannel(const FakeChannel&) = delete;
  FakeChannel& operator=(const FakeChannel&) = delete;

  ~FakeChannel() override;

  mojo::PendingRemote<mojom::Channel> GenerateRemote();
  void DisconnectGeneratedRemote();

  void set_connection_metadata_for_next_call(
      mojom::ConnectionMetadataPtr connection_metadata_for_next_call) {
    connection_metadata_for_next_call_ =
        std::move(connection_metadata_for_next_call);
  }

  std::vector<std::pair<std::string, SendMessageCallback>>& sent_messages() {
    return sent_messages_;
  }

  void SendFileTransferUpdate(int64_t payload_id,
                              mojom::FileTransferStatus status,
                              uint64_t total_bytes,
                              uint64_t bytes_transferred);

  base::flat_map<int64_t, mojo::Remote<mojom::FilePayloadListener>>&
  file_payload_listeners() {
    return file_payload_listeners_;
  }

 private:
  // mojom::Channel:
  void SendMessage(const std::string& message,
                   SendMessageCallback callback) override;
  void RegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      mojo::PendingRemote<mojom::FilePayloadListener> listener,
      RegisterPayloadFileCallback callback) override;
  void GetConnectionMetadata(GetConnectionMetadataCallback callback) override;

  mojo::Receiver<mojom::Channel> receiver_{this};

  std::vector<std::pair<std::string, SendMessageCallback>> sent_messages_;
  mojom::ConnectionMetadataPtr connection_metadata_for_next_call_;
  base::flat_map<int64_t, mojo::Remote<mojom::FilePayloadListener>>
      file_payload_listeners_;
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CHANNEL_H_
