// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBTHEMEENGINE_IMPL_WIN_H_
#define WEBKIT_GLUE_WEBTHEMEENGINE_IMPL_WIN_H_

#include "third_party/WebKit/Source/Platform/chromium/public/win/WebThemeEngine.h"

namespace webkit_glue {

class WebThemeEngineImpl : public WebKit::WebThemeEngine {
 public:
  // WebThemeEngine methods:
  virtual void paintButton(
      WebKit::WebCanvas* canvas, int part, int state, int classic_state,
      const WebKit::WebRect& rect);
  virtual void paintMenuList(
      WebKit::WebCanvas* canvas, int part, int state, int classic_state,
      const WebKit::WebRect& rect);
  virtual void paintScrollbarArrow(
      WebKit::WebCanvas* canvas, int state, int classic_state,
      const WebKit::WebRect& rect);
  virtual void paintScrollbarThumb(
      WebKit::WebCanvas* canvas, int part, int state, int classic_state,
      const WebKit::WebRect& rect);
  virtual void paintScrollbarTrack(
      WebKit::WebCanvas* canvas, int part, int state, int classic_state,
      const WebKit::WebRect& rect, const WebKit::WebRect& align_rect);
  virtual void paintSpinButton(
      WebKit::WebCanvas* canvas, int part, int state, int classic_state,
      const WebKit::WebRect& rect);
  virtual void paintTextField(
      WebKit::WebCanvas* canvas, int part, int state, int classic_state,
      const WebKit::WebRect& rect, WebKit::WebColor color,
      bool fill_content_area, bool draw_edges);
  virtual void paintTrackbar(
      WebKit::WebCanvas* canvas, int part, int state, int classic_state,
      const WebKit::WebRect& rect);
  virtual void paintProgressBar(
      WebKit::WebCanvas* canvas, const WebKit::WebRect& barRect,
      const WebKit::WebRect& valueRect, bool determinate,
      double animatedSeconds);
  virtual WebKit::WebSize getSize(int part);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBTHEMEENGINE_IMPL_WIN_H_
