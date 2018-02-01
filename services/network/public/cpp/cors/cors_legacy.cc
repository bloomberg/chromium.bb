// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/cors_legacy.h"

#include <algorithm>
#include <vector>

#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace {

std::vector<url::Origin>* secure_origins = nullptr;

}  // namespace

namespace network {

namespace cors {

namespace legacy {

void RegisterSecureOrigins(const std::vector<url::Origin>& origins) {
  delete secure_origins;
  secure_origins = new std::vector<url::Origin>(origins.size());
  std::copy(origins.begin(), origins.end(), secure_origins->begin());
}

const std::vector<url::Origin>& GetSecureOrigins() {
  if (!secure_origins)
    secure_origins = new std::vector<url::Origin>;
  return *secure_origins;
}

}  // namespace legacy

}  // namespace cors

}  // namespace network
