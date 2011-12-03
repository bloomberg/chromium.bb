// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBTHEMEENGINE_IMPL_MAC_H_
#define WEBKIT_GLUE_WEBTHEMEENGINE_IMPL_MAC_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/platform/mac/WebThemeEngine.h"

namespace webkit_glue {

class WebThemeEngineImpl : public WebKit::WebThemeEngine {
 public:
  // WebKit::WebThemeEngine implementation.
  virtual void paintScrollbarThumb(
      WebKit::WebCanvas*,
      WebKit::WebThemeEngine::State,
      WebKit::WebThemeEngine::Size,
      const WebKit::WebRect&,
      const WebKit::WebThemeEngine::ScrollbarInfo&);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBTHEMEENGINE_IMPL_MAC_H_
