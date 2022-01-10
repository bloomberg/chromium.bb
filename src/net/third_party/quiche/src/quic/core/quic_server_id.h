// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_QUIC_SERVER_ID_H_
#define QUICHE_QUIC_CORE_QUIC_SERVER_ID_H_

#include <cstdint>
#include <string>

#include "absl/hash/hash.h"
#include "quic/platform/api/quic_export.h"

namespace quic {

// The id used to identify sessions. Includes the hostname, port, scheme and
// privacy_mode.
class QUIC_EXPORT_PRIVATE QuicServerId {
 public:
  QuicServerId();
  QuicServerId(const std::string& host, uint16_t port);
  QuicServerId(const std::string& host,
               uint16_t port,
               bool privacy_mode_enabled);
  ~QuicServerId();

  // Needed to be an element of an ordered container.
  bool operator<(const QuicServerId& other) const;
  bool operator==(const QuicServerId& other) const;

  bool operator!=(const QuicServerId& other) const;

  const std::string& host() const { return host_; }

  uint16_t port() const { return port_; }

  bool privacy_mode_enabled() const { return privacy_mode_enabled_; }

 private:
  std::string host_;
  uint16_t port_;
  bool privacy_mode_enabled_;
};

class QUIC_EXPORT_PRIVATE QuicServerIdHash {
 public:
  size_t operator()(const quic::QuicServerId& server_id) const noexcept {
    return absl::HashOf(server_id.host(), server_id.port(),
                        server_id.privacy_mode_enabled());
  }
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_QUIC_SERVER_ID_H_
