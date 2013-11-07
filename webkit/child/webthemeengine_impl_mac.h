// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WEBTHEMEENGINE_IMPL_MAC_H_
#define WEBKIT_CHILD_WEBTHEMEENGINE_IMPL_MAC_H_

#include "third_party/WebKit/public/platform/mac/WebThemeEngine.h"

namespace webkit_glue {

class WebThemeEngineImpl : public blink::WebThemeEngine {
 public:
  // blink::WebThemeEngine implementation.
  virtual void paintScrollbarThumb(
      blink::WebCanvas* canvas,
      blink::WebThemeEngine::State part,
      blink::WebThemeEngine::Size state,
      const blink::WebRect& rect,
      const blink::WebThemeEngine::ScrollbarInfo& extra_params);
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_WEBTHEMEENGINE_IMPL_MAC_H_
