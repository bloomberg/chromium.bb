// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/simple_webcookiejar_impl.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"

using WebKit::WebString;
using WebKit::WebURL;

void SimpleWebCookieJarImpl::setCookie(const WebURL& url,
                                       const WebURL& first_party_for_cookies,
                                       const WebString& value) {
  SimpleResourceLoaderBridge::SetCookie(
      url, first_party_for_cookies, value.utf8());
}

WebString SimpleWebCookieJarImpl::cookies(
    const WebURL& url,
    const WebURL& first_party_for_cookies) {
  return WebString::fromUTF8(
      SimpleResourceLoaderBridge::GetCookies(url, first_party_for_cookies));
}
