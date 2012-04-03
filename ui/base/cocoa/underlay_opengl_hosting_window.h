// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_UNDERLAY_OPENGL_HOSTING_WINDOW_H_
#define UI_BASE_COCOA_UNDERLAY_OPENGL_HOSTING_WINDOW_H_
#pragma once

#import <Cocoa/Cocoa.h>

// Protocol implemented by windows that need to be informed explicitly about
// underlay surfaces.
@protocol UnderlayableSurface
- (void)underlaySurfaceAdded;
- (void)underlaySurfaceRemoved;
@end

// Common base class for windows that host a OpenGL surface that renders under
// the window. Contains methods relating to hole punching so that the OpenGL
// surface is visible through the window.
@interface UnderlayOpenGLHostingWindow : NSWindow<UnderlayableSurface> {
 @private
  int underlaySurfaceCount_;
}

// Informs the window that an underlay surface has been added/removed. The
// window is non-opaque while underlay surfaces are present.
- (void)underlaySurfaceAdded;
- (void)underlaySurfaceRemoved;

@end

#endif  // UI_BASE_COCOA_UNDERLAY_OPENGL_HOSTING_WINDOW_H_
