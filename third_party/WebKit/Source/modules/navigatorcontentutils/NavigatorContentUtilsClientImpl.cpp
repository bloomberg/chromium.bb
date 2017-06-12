// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/navigatorcontentutils/NavigatorContentUtilsClientImpl.h"

#include "core/frame/WebLocalFrameBase.h"
#include "public/web/WebFrameClient.h"

namespace blink {

NavigatorContentUtilsClientImpl* NavigatorContentUtilsClientImpl::Create(
    WebLocalFrameBase* web_frame) {
  return new NavigatorContentUtilsClientImpl(web_frame);
}

NavigatorContentUtilsClientImpl::NavigatorContentUtilsClientImpl(
    WebLocalFrameBase* web_frame)
    : web_frame_(web_frame) {}

DEFINE_TRACE(NavigatorContentUtilsClientImpl) {
  visitor->Trace(web_frame_);
  NavigatorContentUtilsClient::Trace(visitor);
}

void NavigatorContentUtilsClientImpl::RegisterProtocolHandler(
    const String& scheme,
    const KURL& url,
    const String& title) {
  web_frame_->Client()->RegisterProtocolHandler(scheme, url, title);
}

NavigatorContentUtilsClient::CustomHandlersState
NavigatorContentUtilsClientImpl::IsProtocolHandlerRegistered(
    const String& scheme,
    const KURL& url) {
  return static_cast<NavigatorContentUtilsClient::CustomHandlersState>(
      web_frame_->Client()->IsProtocolHandlerRegistered(scheme, url));
}

void NavigatorContentUtilsClientImpl::UnregisterProtocolHandler(
    const String& scheme,
    const KURL& url) {
  web_frame_->Client()->UnregisterProtocolHandler(scheme, url);
}

}  // namespace blink
