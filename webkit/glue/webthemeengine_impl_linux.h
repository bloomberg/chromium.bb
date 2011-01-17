// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBTHEMEENGINE_IMPL_LINUX_H_
#define WEBTHEMEENGINE_IMPL_LINUX_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/linux/WebThemeEngine.h"

namespace webkit_glue {

class WebThemeEngineImpl : public WebKit::WebThemeEngine {
 public:
  // WebThemeEngine methods:
  virtual WebKit::WebSize getSize(WebKit::WebThemeEngine::Part);
  virtual void paint(
      WebKit::WebCanvas*,
      WebKit::WebThemeEngine::Part,
      WebKit::WebThemeEngine::State,
      const WebKit::WebRect&,
      const WebKit::WebThemeEngine::ExtraParams*);
};

}  // namespace webkit_glue

#endif  // WEBTHEMEENGINE_IMPL_LINUX_H_
