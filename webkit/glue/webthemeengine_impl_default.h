// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBTHEMEENGINE_IMPL_DEFAULT_H_
#define WEBKIT_GLUE_WEBTHEMEENGINE_IMPL_DEFAULT_H_

#include "third_party/WebKit/Source/Platform/chromium/public/default/WebThemeEngine.h"

namespace webkit_glue {

class WebThemeEngineImpl : public WebKit::WebThemeEngine {
 public:
  // WebThemeEngine methods:
  virtual WebKit::WebSize getSize(WebKit::WebThemeEngine::Part);
  virtual void paint(
      WebKit::WebCanvas* canvas,
      WebKit::WebThemeEngine::Part part,
      WebKit::WebThemeEngine::State state,
      const WebKit::WebRect& rect,
      const WebKit::WebThemeEngine::ExtraParams* extra_params);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBTHEMEENGINE_IMPL_DEFAULT_H_
