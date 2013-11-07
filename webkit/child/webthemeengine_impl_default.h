// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WEBTHEMEENGINE_IMPL_DEFAULT_H_
#define WEBKIT_CHILD_WEBTHEMEENGINE_IMPL_DEFAULT_H_

#include "third_party/WebKit/public/platform/default/WebThemeEngine.h"

namespace webkit_glue {

class WebThemeEngineImpl : public blink::WebThemeEngine {
 public:
  // WebThemeEngine methods:
  virtual blink::WebSize getSize(blink::WebThemeEngine::Part);
  virtual void paint(
      blink::WebCanvas* canvas,
      blink::WebThemeEngine::Part part,
      blink::WebThemeEngine::State state,
      const blink::WebRect& rect,
      const blink::WebThemeEngine::ExtraParams* extra_params);
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_WEBTHEMEENGINE_IMPL_DEFAULT_H_
