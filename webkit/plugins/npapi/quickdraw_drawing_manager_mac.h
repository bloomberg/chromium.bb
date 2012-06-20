// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_QUICKDRAW_DRAWING_MANAGER_MAC_H_
#define WEBKIT_PLUGINS_NPAPI_QUICKDRAW_DRAWING_MANAGER_MAC_H_

#ifndef NP_NO_QUICKDRAW

#import <Carbon/Carbon.h>

#include "ui/gfx/rect.h"

namespace webkit {
namespace npapi {

// Plugin helper class encapsulating the details of capturing what a QuickDraw
// drawing model plugin draws, then drawing it into a CGContext.
class QuickDrawDrawingManager {
 public:
  QuickDrawDrawingManager();
  ~QuickDrawDrawingManager();

  // Sets the context that the plugin bits should be copied into when
  // UpdateContext is called. This object does not retain |context|, so the
  // caller must call SetTargetContext again if the context changes.
  void SetTargetContext(CGContextRef context, const gfx::Size& plugin_size);

  // Sets the window that is used by the plugin. This object does not own the
  // window, so the caler must call SetPluginWindow again if the window changes.
  void SetPluginWindow(WindowRef window);

  // Updates the target context with the current plugin bits.
  void UpdateContext();

  // Returns the port that the plugin should draw into. This returned port is
  // only valid until the next call to SetPluginWindow.
  CGrafPtr port() { return current_port_; }

  // Makes the QuickDraw port current; should be called before calls where the
  // plugin might draw.
  void MakePortCurrent();

 private:
  // Scrapes the contents of the window into the given context.
  static void ScrapeWindow(WindowRef window, CGContextRef target_context,
                           const gfx::Size& plugin_size);

  WindowRef plugin_window_;  // Weak reference.
  CGContextRef target_context_;  // Weak reference.
  gfx::Size plugin_size_;
  CGrafPtr current_port_;
};

}  // namespace npapi
}  // namespace webkit

#endif  // !NP_NO_QUICKDRAW

#endif  // WEBKIT_PLUGINS_NPAPI_QUICKDRAW_DRAWING_MANAGER_MAC_H_
