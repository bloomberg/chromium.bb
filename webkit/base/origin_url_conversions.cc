// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/base/origin_url_conversions.h"

#include "base/strings/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

namespace webkit_base {

GURL GetOriginURLFromIdentifier(const std::string& identifier) {
  WebKit::WebSecurityOrigin web_security_origin =
      WebKit::WebSecurityOrigin::createFromDatabaseIdentifier(
          WebKit::WebString::fromUTF8(identifier));

  // We need this work-around for file:/// URIs as
  // createFromDatabaseIdentifier returns null origin_url for them.
  if (web_security_origin.isUnique()) {
    if (identifier.find("file__") == 0)
      return GURL("file:///");
    return GURL();
  }

  return GURL(web_security_origin.toString());
}

std::string GetOriginIdentifierFromURL(const GURL& url) {
  return WebKit::WebSecurityOrigin::createFromString(
      UTF8ToUTF16(url.spec())).databaseIdentifier().utf8();
}

}  // namespace webkit_base
