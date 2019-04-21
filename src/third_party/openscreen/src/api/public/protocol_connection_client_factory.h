// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PROTOCOL_CONNECTION_CLIENT_FACTORY_H_
#define API_PUBLIC_PROTOCOL_CONNECTION_CLIENT_FACTORY_H_

#include <memory>

#include "api/public/protocol_connection_client.h"

namespace openscreen {

class ProtocolConnectionClientFactory {
 public:
  static std::unique_ptr<ProtocolConnectionClient> Create(
      MessageDemuxer* demuxer,
      ProtocolConnectionServiceObserver* observer);
};

}  // namespace openscreen

#endif  // API_PUBLIC_PROTOCOL_CONNECTION_CLIENT_FACTORY_H_
