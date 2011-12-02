// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A struct for managing data being dropped on a webview.  This represents a
// union of all the types of data that can be dropped in a platform neutral
// way.

#ifndef WEBKIT_GLUE_WEBCOOKIE_H_
#define WEBKIT_GLUE_WEBCOOKIE_H_

#include "net/base/cookie_monster.h"
#include "webkit/glue/webkit_glue_export.h"

namespace webkit_glue {

struct WEBKIT_GLUE_EXPORT WebCookie {
  WebCookie();
  explicit WebCookie(const net::CookieMonster::CanonicalCookie& c);
  WebCookie(const std::string& name, const std::string& value,
            const std::string& domain, const std::string& path, double expires,
            bool http_only, bool secure, bool session);
  ~WebCookie();

  // Cookie name.
  std::string name;

  // Cookie value.
  std::string value;

  // Cookie domain.
  std::string domain;

  // Cookie path.
  std::string path;

  // Cookie expires param if any.
  double expires;

  // Cookie HTTPOnly param.
  bool http_only;

  // Cookie secure param.
  bool secure;

  // Session cookie flag.
  bool session;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBCOOKIE_H_
