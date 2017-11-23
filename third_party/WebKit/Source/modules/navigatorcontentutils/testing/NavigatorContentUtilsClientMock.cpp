// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/navigatorcontentutils/testing/NavigatorContentUtilsClientMock.h"

#include "modules/navigatorcontentutils/NavigatorContentUtilsClient.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

void NavigatorContentUtilsClientMock::RegisterProtocolHandler(
    const String& scheme,
    const KURL& url,
    const String& title) {
  ProtocolInfo info;
  info.scheme = scheme;
  info.url = url;
  info.title = title;

  protocol_map_.Set(scheme, info);
}

void NavigatorContentUtilsClientMock::UnregisterProtocolHandler(
    const String& scheme,
    const KURL& url) {
  protocol_map_.erase(scheme);
}

}  // namespace blink
