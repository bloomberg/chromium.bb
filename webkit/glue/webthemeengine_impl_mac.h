// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBTHEMEENGINE_IMPL_MAC_H_
#define WEBKIT_GLUE_WEBTHEMEENGINE_IMPL_MAC_H_

#include "third_party/WebKit/Source/Platform/chromium/public/mac/WebThemeEngine.h"

namespace webkit_glue {

class WebThemeEngineImpl : public WebKit::WebThemeEngine {
 public:
  // WebKit::WebThemeEngine implementation.
  virtual void paintScrollbarThumb(
      WebKit::WebCanvas* canvas,
      WebKit::WebThemeEngine::State part,
      WebKit::WebThemeEngine::Size state,
      const WebKit::WebRect& rect,
      const WebKit::WebThemeEngine::ScrollbarInfo& extra_params);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBTHEMEENGINE_IMPL_MAC_H_
