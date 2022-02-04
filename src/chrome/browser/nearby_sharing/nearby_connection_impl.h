// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTION_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTION_IMPL_H_

#include "chrome/browser/nearby_sharing/nearby_connection.h"

#include <queue>
#include <vector>

#include "ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "base/sequence_checker.h"

class NearbyConnectionsManager;

class NearbyConnectionImpl : public NearbyConnection {
 public:
  NearbyConnectionImpl(NearbyConnectionsManager* nearby_connections_manager,
                       const std::string& endpoint_id);
  ~NearbyConnectionImpl() override;

  // NearbyConnection:
  void Read(ReadCallback callback) override;
  void Write(std::vector<uint8_t> bytes) override;
  void Close() override;
  void SetDisconnectionListener(base::OnceClosure listener) override;

  // Add |bytes| to the read queue, notifying ReadCallback.
  void WriteMessage(std::vector<uint8_t> bytes);

 private:
  using PayloadContent = location::nearby::connections::mojom::PayloadContent;
  using BytesPayload = location::nearby::connections::mojom::BytesPayload;

  SEQUENCE_CHECKER(sequence_checker_);

  NearbyConnectionsManager* nearby_connections_manager_;
  std::string endpoint_id_;
  ReadCallback read_callback_;
  base::OnceClosure disconnect_listener_;

  // A read queue. The data that we've read from the remote device ends up here
  // until Read() is called to dequeue it.
  std::queue<std::vector<uint8_t>> reads_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTION_IMPL_H_
