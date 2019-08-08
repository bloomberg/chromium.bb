// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_COOKIE_MANAGERS_SHARED_H_
#define SERVICES_NETWORK_COOKIE_MANAGERS_SHARED_H_

#include "net/cookies/cookie_store.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace net {
enum class CookieChangeCause;
}

namespace network {

mojom::CookieChangeCause ToCookieChangeCause(net::CookieChangeCause net_cause);

}  // namespace network

#endif  // SERVICES_NETWORK_COOKIE_MANAGERS_SHARED_H_
