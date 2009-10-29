// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A struct for managing data being dropped on a webview.  This represents a
// union of all the types of data that can be dropped in a platform neutral
// way.

#ifndef WEBKIT_GLUE_WEBCOOKIE_H_
#define WEBKIT_GLUE_WEBCOOKIE_H_

#include <string>

namespace webkit_glue {

struct WebCookie {

  WebCookie(const std::string& name, const std::string& value,
            const std::string& domain, const std::string& path, double expires,
            bool http_only, bool secure, bool session)
    : name(name),
      value(value),
      domain(domain),
      path(path),
      expires(expires),
      http_only(http_only),
      secure(secure),
      session(session) {
  }

  // For default constructions.
  WebCookie() :
    expires(0),
    http_only(false),
    secure(false),
    session(false) {
  }

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
