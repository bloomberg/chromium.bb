// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFactory_h
#define WebFactory_h

#include "core/CoreExport.h"
#include "public/platform/WebPageVisibilityState.h"

namespace blink {

class ChromeClient;
class WebViewBase;
class WebViewClient;

// WebFactory is a temporary class implemented in web/ that allows classes to
// construct interfaces that are being moved out of web/.
// This class will be removed once all implementations are in core/ or modules/.
class CORE_EXPORT WebFactory {
 public:
  static WebFactory& GetInstance();

  virtual ChromeClient* CreateChromeClient(WebViewBase*) const = 0;
  virtual WebViewBase* CreateWebViewBase(WebViewClient*,
                                         WebPageVisibilityState) const = 0;

 protected:
  // Takes ownership of |factory|.
  static void SetInstance(WebFactory&);

 private:
  static WebFactory* factory_instance_;
};

}  // namespace blink

#endif
