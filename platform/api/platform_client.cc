// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/platform_client.h"

namespace openscreen {
namespace platform {

PlatformClient::~PlatformClient() = default;
PlatformClient::PlatformClient() = default;

// static
void PlatformClient::ShutDown() {
  delete client_;
}

// static
void PlatformClient::SetInstance(PlatformClient* client) {
  client_ = client;
}

// static
PlatformClient* PlatformClient::client_ = nullptr;

}  // namespace platform
}  // namespace openscreen
