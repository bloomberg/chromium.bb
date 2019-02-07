// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_cookie_manager.h"

#include <utility>

namespace network {

TestCookieManager::TestCookieManager() = default;

TestCookieManager::~TestCookieManager() = default;

void TestCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    bool secure_source,
    bool modify_http_only,
    SetCanonicalCookieCallback callback) {
  std::move(callback).Run(false);
}

void TestCookieManager::AddCookieChangeListener(
    const GURL& url,
    const std::string& name,
    network::mojom::CookieChangeListenerPtr listener) {
  cookie_change_listeners_.push_back(std::move(listener));
}

}  // namespace network
