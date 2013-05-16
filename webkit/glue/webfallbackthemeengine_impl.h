// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBFALLBACKTHEMEENGINE_IMPL_H_
#define WEBKIT_GLUE_WEBFALLBACKTHEMEENGINE_IMPL_H_

#include "third_party/WebKit/Source/Platform/chromium/public/WebFallbackThemeEngine.h"

namespace webkit_glue {

class WebFallbackThemeEngineImpl : public WebKit::WebFallbackThemeEngine {
 public:
  // WebFallbackThemeEngine methods:
  virtual WebKit::WebSize getSize(WebKit::WebFallbackThemeEngine::Part);
  virtual void paint(
      WebKit::WebCanvas* canvas,
      WebKit::WebFallbackThemeEngine::Part part,
      WebKit::WebFallbackThemeEngine::State state,
      const WebKit::WebRect& rect,
      const WebKit::WebFallbackThemeEngine::ExtraParams* extra_params);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBFALLBACKTHEMEENGINE_IMPL_H_
