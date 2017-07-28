// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/WebFactoryImpl.h"
#include "core/exported/WebViewImpl.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/page/ChromeClientImpl.h"

namespace blink {

void WebFactoryImpl::Initialize() {
  WebFactory::SetInstance(*(new WebFactoryImpl()));
}

ChromeClient* WebFactoryImpl::CreateChromeClient(WebViewBase* view) const {
  return ChromeClientImpl::Create(view);
}

WebViewBase* WebFactoryImpl::CreateWebViewBase(
    WebViewClient* client,
    WebPageVisibilityState state) const {
  return WebViewImpl::Create(client, state);
}

WebLocalFrameBase* WebFactoryImpl::CreateMainWebLocalFrameBase(
    WebView* web_view,
    WebFrameClient* client,
    InterfaceRegistry* registry) const {
  return WebLocalFrameImpl::CreateMainFrame(web_view, client, registry, nullptr,
                                            g_empty_atom,
                                            WebSandboxFlags::kNone);
}

WebLocalFrameBase* WebFactoryImpl::CreateWebLocalFrameBase(
    WebTreeScopeType type,
    WebFrameClient* client,
    InterfaceRegistry* registry,
    WebFrame* opener) const {
  return WebLocalFrameImpl::Create(type, client, registry, opener);
}

}  // namespace blink
