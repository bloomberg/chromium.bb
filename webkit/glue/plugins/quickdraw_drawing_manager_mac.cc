// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NP_NO_QUICKDRAW

#include "webkit/glue/plugins/quickdraw_drawing_manager_mac.h"

#include "webkit/glue/plugins/coregraphics_private_symbols_mac.h"

// Turn off GCC warnings about deprecated functions (since QuickDraw is a
// deprecated API). According to the GCC documentation, this can only be done
// per file, not pushed and popped like some options can be.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

QuickDrawDrawingManager::QuickDrawDrawingManager()
    : plugin_window_(NULL), target_context_(NULL), fast_path_enabled_(false),
      current_port_(NULL), target_world_(NULL), plugin_world_(NULL) {}

QuickDrawDrawingManager::~QuickDrawDrawingManager() {
  DestroyGWorlds();
}

void QuickDrawDrawingManager::SetFastPathEnabled(bool enabled) {
  if (fast_path_enabled_ == enabled)
    return;

  fast_path_enabled_ = enabled;
  if (enabled) {
    if (!target_world_)
      UpdateGWorlds();
    // Copy our last window snapshot into our new source, since the plugin
    // may not repaint everything.
    CopyGWorldBits(target_world_, plugin_world_, plugin_size_);
    current_port_ = plugin_world_;
  } else {
    current_port_ = GetWindowPort(plugin_window_);
  }
}

void QuickDrawDrawingManager::SetTargetContext(CGContextRef context,
                                               const gfx::Size& plugin_size) {
  target_context_ = context;
  if (plugin_size != plugin_size_) {
    plugin_size_ = plugin_size;
    // Pitch the old GWorlds, since they are the wrong size now.
    DestroyGWorlds();
    if (fast_path_enabled_)
      UpdateGWorlds();
  }
}

void QuickDrawDrawingManager::SetPluginWindow(WindowRef window) {
  plugin_window_ = window;
  if (!fast_path_enabled_)
    current_port_ = GetWindowPort(window);
}

void QuickDrawDrawingManager::UpdateContext() {
  if (fast_path_enabled_)
    CopyGWorldBits(plugin_world_, target_world_, plugin_size_);
  else
    ScrapeWindow(plugin_window_, target_context_, plugin_size_);
}

bool QuickDrawDrawingManager::IsFastPathEnabled() {
  return fast_path_enabled_;
}

void QuickDrawDrawingManager::MakePortCurrent() {
  if (fast_path_enabled_)
    SetGWorld(current_port_, NULL);
  else
    SetPort(current_port_);
}

void QuickDrawDrawingManager::DestroyGWorlds() {
  if (plugin_world_) {
    DisposeGWorld(plugin_world_);
    plugin_world_ = NULL;
  }
  if (target_world_) {
    DisposeGWorld(target_world_);
    target_world_ = NULL;
  }
}

void QuickDrawDrawingManager::UpdateGWorlds() {
  DestroyGWorlds();
  if (!target_context_)
    return;

  Rect window_bounds = {
    0, 0, plugin_size_.height(), plugin_size_.width()
  };
  // Create a GWorld pointing at the same bits as our target context.
  if (target_context_) {
    NewGWorldFromPtr(
        &target_world_, k32BGRAPixelFormat, &window_bounds, NULL, NULL, 0,
        static_cast<Ptr>(CGBitmapContextGetData(target_context_)),
        static_cast<SInt32>(CGBitmapContextGetBytesPerRow(target_context_)));
  }
  // Create a GWorld for the plugin to paint into whenever it wants; since
  // QuickDraw plugins don't draw at known times, they can't be allowed to draw
  // directly into the shared memory.
  NewGWorld(&plugin_world_, k32ARGBPixelFormat, &window_bounds,
            NULL, NULL, kNativeEndianPixMap);
  if (fast_path_enabled_)
    current_port_ = plugin_world_;
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

void QuickDrawDrawingManager::CopyGWorldBits(GWorldPtr source, GWorldPtr dest,
                                             const gfx::Size& plugin_size) {
  if (!(source && dest))
    return;

  Rect window_bounds = { 0, 0, plugin_size.height(), plugin_size.width() };
  PixMapHandle source_pixmap = GetGWorldPixMap(source);
  if (LockPixels(source_pixmap)) {
    PixMapHandle dest_pixmap = GetGWorldPixMap(dest);
    if (LockPixels(dest_pixmap)) {
      SetGWorld(dest, NULL);
      // Set foreground and background colors to avoid "colorizing" the image.
      ForeColor(blackColor);
      BackColor(whiteColor);
      CopyBits(reinterpret_cast<BitMap*>(*source_pixmap),
               reinterpret_cast<BitMap*>(*dest_pixmap),
               &window_bounds, &window_bounds, srcCopy, NULL);
      UnlockPixels(dest_pixmap);
    }
    UnlockPixels(source_pixmap);
  }
}

#endif  // !NP_NO_QUICKDRAW
