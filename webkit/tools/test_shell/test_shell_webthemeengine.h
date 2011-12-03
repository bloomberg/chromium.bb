// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TestShellWebTheme::Engine implements the WebThemeEngine
// API used by the Windows version of Chromium to render native form
// controls like checkboxes, radio buttons, and scroll bars. The normal
// implementation (native_theme) renders the controls using either the
// UXTheme theming engine present in XP, Vista, and Win 7, or the "classic"
// theme used if that theme is selected in the Desktop settings.
// Unfortunately, both of these themes render controls differently on the
// different versions of Windows.
//
// In order to ensure maximum consistency of baselines across the different
// Windows versions, we provide a simple implementation for test_shell here
// instead. These controls are actually platform-independent (they're rendered
// using Skia) and could be used on Linux and the Mac as well, should we
// choose to do so at some point.
//

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBTHEMEENGINE_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBTHEMEENGINE_H_

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/win/WebThemeEngine.h"

namespace TestShellWebTheme {

class Engine : public WebKit::WebThemeEngine {
 public:
  Engine() {}

  // WebThemeEngine methods:
  virtual void paintButton(
      WebKit::WebCanvas*, int part, int state, int classic_state,
      const WebKit::WebRect&);
  virtual void paintMenuList(
      WebKit::WebCanvas*, int part, int state, int classic_state,
      const WebKit::WebRect&);
  virtual void paintScrollbarArrow(
      WebKit::WebCanvas*, int state, int classic_state,
      const WebKit::WebRect&);
  virtual void paintScrollbarThumb(
      WebKit::WebCanvas*, int part, int state, int classic_state,
      const WebKit::WebRect&);
  virtual void paintScrollbarTrack(
      WebKit::WebCanvas*, int part, int state, int classic_state,
      const WebKit::WebRect&, const WebKit::WebRect& align_rect);
  virtual void paintTextField(
      WebKit::WebCanvas*, int part, int state, int classic_state,
      const WebKit::WebRect&, WebKit::WebColor, bool fill_content_area,
      bool draw_edges);
  virtual void paintTrackbar(
      WebKit::WebCanvas*, int part, int state, int classic_state,
      const WebKit::WebRect&);
  virtual void paintProgressBar(
      WebKit::WebCanvas*, const WebKit::WebRect& barRect,
      const WebKit::WebRect& valueRect,
      bool determinate, double time);

 private:
  DISALLOW_COPY_AND_ASSIGN(Engine);
};

}  // namespace TestShellWebTheme

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBTHEMEENGINE_H_

