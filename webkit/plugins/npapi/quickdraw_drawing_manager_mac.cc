// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NP_NO_QUICKDRAW

#include "webkit/plugins/npapi/quickdraw_drawing_manager_mac.h"

#include "webkit/plugins/npapi/coregraphics_private_symbols_mac.h"

// Turn off GCC warnings about deprecated functions (since QuickDraw is a
// deprecated API). According to the GCC documentation, this can only be done
// per file, not pushed and popped like some options can be.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace webkit {
namespace npapi {

QuickDrawDrawingManager::QuickDrawDrawingManager()
    : plugin_window_(NULL), target_context_(NULL), current_port_(NULL) {}

QuickDrawDrawingManager::~QuickDrawDrawingManager() {
}

void QuickDrawDrawingManager::SetTargetContext(CGContextRef context,
                                               const gfx::Size& plugin_size) {
  target_context_ = context;
  plugin_size_ = plugin_size;
}

void QuickDrawDrawingManager::SetPluginWindow(WindowRef window) {
  plugin_window_ = window;
  current_port_ = GetWindowPort(window);
}

void QuickDrawDrawingManager::UpdateContext() {
  ScrapeWindow(plugin_window_, target_context_, plugin_size_);
}

void QuickDrawDrawingManager::MakePortCurrent() {
  SetPort(current_port_);
}

void QuickDrawDrawingManager::ScrapeWindow(WindowRef window,
                                           CGContextRef target_context,
                                           const gfx::Size& plugin_size) {
  if (!target_context)
    return;

  CGRect window_bounds = CGRectMake(0, 0,
                                    plugin_size.width(),
                                    plugin_size.height());
  CGWindowID window_id = HIWindowGetCGWindowID(window);
  CGContextSaveGState(target_context);
  CGContextTranslateCTM(target_context, 0, plugin_size.height());
  CGContextScaleCTM(target_context, 1.0, -1.0);
  CGContextCopyWindowCaptureContentsToRect(target_context, window_bounds,
                                           _CGSDefaultConnection(),
                                           window_id, 0);
  CGContextRestoreGState(target_context);
}

}  // namespace npapi
}  // namespace webkit

#endif  // !NP_NO_QUICKDRAW
