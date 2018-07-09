// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_utils.h"

#include "services/network/public/cpp/resource_request.h"

namespace network {

std::string GetUploadData(const network::ResourceRequest& request) {
  auto body = request.request_body;
  if (!body)
    return std::string();

  CHECK_EQ(1u, body->elements()->size());
  const auto& element = body->elements()->at(0);
  CHECK_EQ(network::DataElement::TYPE_BYTES, element.type());
  return std::string(element.bytes(), element.length());
}

}  // namespace network
