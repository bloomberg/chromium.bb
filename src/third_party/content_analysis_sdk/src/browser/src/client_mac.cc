// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "client_mac.h"

namespace content_analysis {
namespace sdk {

// static
std::unique_ptr<Client> Client::Create(Config config) {
  return std::make_unique<ClientMac>(std::move(config));
}

ClientMac::ClientMac(Config config) : ClientBase(std::move(config)) {}

int ClientMac::Send(const ContentAnalysisRequest& request,
                    ContentAnalysisResponse* response) {
  return -1;
}

}  // namespace sdk
}  // namespace content_analysis