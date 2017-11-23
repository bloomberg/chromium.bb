// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/navigatorcontentutils/NavigatorContentUtilsClient.h"

#include "core/frame/WebLocalFrameImpl.h"
#include "platform/weborigin/KURL.h"
#include "public/web/WebFrameClient.h"

namespace blink {

NavigatorContentUtilsClient* NavigatorContentUtilsClient::Create(
    WebLocalFrameImpl* web_frame) {
  return new NavigatorContentUtilsClient(web_frame);
}

NavigatorContentUtilsClient::NavigatorContentUtilsClient(
    WebLocalFrameImpl* web_frame)
    : web_frame_(web_frame) {}

void NavigatorContentUtilsClient::Trace(blink::Visitor* visitor) {
  visitor->Trace(web_frame_);
}

void NavigatorContentUtilsClient::RegisterProtocolHandler(const String& scheme,
                                                          const KURL& url,
                                                          const String& title) {
  web_frame_->Client()->RegisterProtocolHandler(scheme, url, title);
}

void NavigatorContentUtilsClient::UnregisterProtocolHandler(
    const String& scheme,
    const KURL& url) {
  web_frame_->Client()->UnregisterProtocolHandler(scheme, url);
}

}  // namespace blink
