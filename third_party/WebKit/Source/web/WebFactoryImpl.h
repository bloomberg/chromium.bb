// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFactoryImpl_h
#define WebFactoryImpl_h

#include "core/exported/WebFactory.h"
#include "web/WebExport.h"

namespace blink {

class WebFactoryImpl : public WebFactory {
 public:
  WebFactoryImpl() {}
  ~WebFactoryImpl() {}

  // Sets WebFactory to have a new instance of WebFactoryImpl.
  WEB_EXPORT static void Initialize();

  ChromeClient* CreateChromeClient(WebViewBase*) const override;
  WebViewBase* CreateWebViewBase(WebViewClient*,
                                 WebPageVisibilityState) const override;
  WebLocalFrameBase* CreateMainWebLocalFrameBase(
      WebView*,
      WebFrameClient*,
      InterfaceRegistry*) const override;
  WebLocalFrameBase* CreateWebLocalFrameBase(WebTreeScopeType,
                                             WebFrameClient*,
                                             InterfaceRegistry*,
                                             WebFrame* opener) const override;

};

}  // namespace blink

#endif
