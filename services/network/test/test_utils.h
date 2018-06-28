// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_UTILS_H_
#define SERVICES_NETWORK_TEST_TEST_UTILS_H_

#include <string>

namespace network {
struct ResourceRequest;

// Helper method to read the upload bytes from a ResourceRequest with a body.
std::string GetUploadData(const network::ResourceRequest& request);

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_UTILS_H_
