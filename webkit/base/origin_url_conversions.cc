// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/base/origin_url_conversions.h"

#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"

namespace webkit_base {

GURL GetOriginURLFromIdentifier(const base::string16& identifier) {
  WebKit::WebSecurityOrigin web_security_origin =
      WebKit::WebSecurityOrigin::createFromDatabaseIdentifier(identifier);

  // We need this work-around for file:/// URIs as
  // createFromDatabaseIdentifier returns null origin_url for them.
  if (web_security_origin.isUnique()) {
    if (identifier.find(UTF8ToUTF16("file__")) == 0)
      return GURL("file:///");
    return GURL();
  }

  return GURL(web_security_origin.toString());
}

base::string16 GetOriginIdentifierFromURL(const GURL& url) {
  return WebKit::WebSecurityOrigin::createFromString(
      UTF8ToUTF16(url.spec())).databaseIdentifier();
}

}  // namespace webkit_base
