// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WEBTHEMEENGINE_IMPL_WIN_H_
#define WEBKIT_CHILD_WEBTHEMEENGINE_IMPL_WIN_H_

#include "third_party/WebKit/public/platform/win/WebThemeEngine.h"

namespace webkit_glue {

class WebThemeEngineImpl : public blink::WebThemeEngine {
 public:
  // WebThemeEngine methods:
  virtual void paintButton(
      blink::WebCanvas* canvas, int part, int state, int classic_state,
      const blink::WebRect& rect);
  virtual void paintMenuList(
      blink::WebCanvas* canvas, int part, int state, int classic_state,
      const blink::WebRect& rect);
  virtual void paintScrollbarArrow(
      blink::WebCanvas* canvas, int state, int classic_state,
      const blink::WebRect& rect);
  virtual void paintScrollbarThumb(
      blink::WebCanvas* canvas, int part, int state, int classic_state,
      const blink::WebRect& rect);
  virtual void paintScrollbarTrack(
      blink::WebCanvas* canvas, int part, int state, int classic_state,
      const blink::WebRect& rect, const blink::WebRect& align_rect);
  virtual void paintSpinButton(
      blink::WebCanvas* canvas, int part, int state, int classic_state,
      const blink::WebRect& rect);
  virtual void paintTextField(
      blink::WebCanvas* canvas, int part, int state, int classic_state,
      const blink::WebRect& rect, blink::WebColor color,
      bool fill_content_area, bool draw_edges);
  virtual void paintTrackbar(
      blink::WebCanvas* canvas, int part, int state, int classic_state,
      const blink::WebRect& rect);
  virtual void paintProgressBar(
      blink::WebCanvas* canvas, const blink::WebRect& barRect,
      const blink::WebRect& valueRect, bool determinate,
      double animatedSeconds);
  virtual blink::WebSize getSize(int part);
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_WEBTHEMEENGINE_IMPL_WIN_H_
