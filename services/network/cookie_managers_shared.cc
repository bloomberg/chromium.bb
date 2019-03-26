// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_managers_shared.h"

#include <utility>

#include "base/bind.h"
#include "net/cookies/cookie_change_dispatcher.h"

namespace network {

mojom::CookieChangeCause ToCookieChangeCause(net::CookieChangeCause net_cause) {
  switch (net_cause) {
    case net::CookieChangeCause::INSERTED:
      return mojom::CookieChangeCause::INSERTED;
    case net::CookieChangeCause::EXPLICIT:
      return mojom::CookieChangeCause::EXPLICIT;
    case net::CookieChangeCause::UNKNOWN_DELETION:
      return mojom::CookieChangeCause::UNKNOWN_DELETION;
    case net::CookieChangeCause::OVERWRITE:
      return mojom::CookieChangeCause::OVERWRITE;
    case net::CookieChangeCause::EXPIRED:
      return mojom::CookieChangeCause::EXPIRED;
    case net::CookieChangeCause::EVICTED:
      return mojom::CookieChangeCause::EVICTED;
    case net::CookieChangeCause::EXPIRED_OVERWRITE:
      return mojom::CookieChangeCause::EXPIRED_OVERWRITE;
  }
  NOTREACHED();
  return mojom::CookieChangeCause::EXPLICIT;
}

}  // namespace network
