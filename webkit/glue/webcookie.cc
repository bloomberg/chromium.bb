// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webcookie.h"

namespace webkit_glue {

WebCookie::WebCookie()
    : expires(0),
      http_only(false),
      secure(false),
      session(false) {
}

WebCookie::WebCookie(const net::CookieMonster::CanonicalCookie& c)
    : name(c.Name()),
      value(c.Value()),
      domain(c.Domain()),
      path(c.Path()),
      expires(c.ExpiryDate().ToDoubleT() * 1000),
      http_only(c.IsHttpOnly()),
      secure(c.IsSecure()),
      session(!c.IsPersistent()) {
}

WebCookie::WebCookie(const std::string& name, const std::string& value,
                     const std::string& domain, const std::string& path,
                     double expires, bool http_only, bool secure, bool session)
    : name(name),
      value(value),
      domain(domain),
      path(path),
      expires(expires),
      http_only(http_only),
      secure(secure),
      session(session) {
}

WebCookie::~WebCookie() {
}

}  // namespace webkit_glue
