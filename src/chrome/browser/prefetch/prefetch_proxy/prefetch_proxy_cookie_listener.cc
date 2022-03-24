// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_cookie_listener.h"

#include "base/check.h"

// static
std::unique_ptr<PrefetchProxyCookieListener>
PrefetchProxyCookieListener::MakeAndRegister(
    const GURL& url,
    network::mojom::CookieManager* cookie_manager) {
  DCHECK(cookie_manager);

  std::unique_ptr<PrefetchProxyCookieListener> listener =
      std::make_unique<PrefetchProxyCookieListener>(url);

  // |listener| will get updates whenever host cookies for |url| or
  // domain cookies that match |url| are changed.
  cookie_manager->AddCookieChangeListener(
      url, absl::nullopt,
      listener->cookie_listener_receiver_.BindNewPipeAndPassRemote());

  return listener;
}

PrefetchProxyCookieListener::PrefetchProxyCookieListener(const GURL& url)
    : url_(url) {}

PrefetchProxyCookieListener::~PrefetchProxyCookieListener() = default;

void PrefetchProxyCookieListener::StopListening() {
  cookie_listener_receiver_.reset();
}

void PrefetchProxyCookieListener::OnCookieChange(
    const net::CookieChangeInfo& change) {
  DCHECK(url_.DomainIs(change.cookie.DomainWithoutDot()));
  have_cookies_changed_ = true;

  // Once we record one change to the cookies associated with |url_|, we don't
  // care about any subsequent changes.
  StopListening();
}
