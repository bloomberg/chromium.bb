// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WEBFALLBACKTHEMEENGINE_IMPL_H_
#define WEBKIT_CHILD_WEBFALLBACKTHEMEENGINE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebFallbackThemeEngine.h"

namespace ui {
class FallbackTheme;
}

namespace webkit_glue {

class WebFallbackThemeEngineImpl : public WebKit::WebFallbackThemeEngine {
 public:
  WebFallbackThemeEngineImpl();
  virtual ~WebFallbackThemeEngineImpl();

  // WebFallbackThemeEngine methods:
  virtual WebKit::WebSize getSize(WebKit::WebFallbackThemeEngine::Part);
  virtual void paint(
      WebKit::WebCanvas* canvas,
      WebKit::WebFallbackThemeEngine::Part part,
      WebKit::WebFallbackThemeEngine::State state,
      const WebKit::WebRect& rect,
      const WebKit::WebFallbackThemeEngine::ExtraParams* extra_params);

 private:
  scoped_ptr<ui::FallbackTheme> theme_;

  DISALLOW_COPY_AND_ASSIGN(WebFallbackThemeEngineImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_WEBFALLBACKTHEMEENGINE_IMPL_H_
