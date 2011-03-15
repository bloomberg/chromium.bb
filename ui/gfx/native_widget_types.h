// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_WIDGET_TYPES_H_
#define UI_GFX_NATIVE_WIDGET_TYPES_H_
#pragma once

#include "base/basictypes.h"
#include "build/build_config.h"

// This file provides cross platform typedefs for native widget types.
//   NativeWindow: this is a handle to a native, top-level window
//   NativeView: this is a handle to a native UI element. It may be the
//     same type as a NativeWindow on some platforms.
//   NativeViewId: Often, in our cross process model, we need to pass around a
//     reference to a "window". This reference will, say, be echoed back from a
//     renderer to the browser when it wishes to query its size. On Windows we
//     use an HWND for this.
//
//     As a rule of thumb - if you're in the renderer, you should be dealing
//     with NativeViewIds. This should remind you that you shouldn't be doing
//     direct operations on platform widgets from the renderer process.
//
//     If you're in the browser, you're probably dealing with NativeViews,
//     unless you're in the IPC layer, which will be translating between
//     NativeViewIds from the renderer and NativeViews.
//
//   NativeEditView: a handle to a native edit-box. The Mac folks wanted this
//     specific typedef.
//
//   NativeImage: The platform-specific image type used for drawing UI elements
//     in the browser.
//
// The name 'View' here meshes with OS X where the UI elements are called
// 'views' and with our Chrome UI code where the elements are also called
// 'views'.

#if defined(OS_WIN)
#include <windows.h>  // NOLINT
typedef struct HFONT__* HFONT;
#elif defined(OS_MACOSX)
struct CGContext;
#ifdef __OBJC__
@class NSFont;
@class NSImage;
@class NSView;
@class NSWindow;
@class NSTextField;
#else
class NSFont;
class NSImage;
class NSView;
class NSWindow;
class NSTextField;
#endif  // __OBJC__
#elif defined(TOOLKIT_USES_GTK)
typedef struct _PangoFontDescription PangoFontDescription;
typedef struct _GdkCursor GdkCursor;
typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GdkRegion GdkRegion;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;
typedef struct _cairo cairo_t;
#endif
class SkBitmap;

namespace gfx {

#if defined(OS_WIN)
typedef HFONT NativeFont;
typedef HWND NativeView;
typedef HWND NativeWindow;
typedef HWND NativeEditView;
typedef HDC NativeDrawingContext;
typedef HCURSOR NativeCursor;
typedef HMENU NativeMenu;
typedef HRGN NativeRegion;
#elif defined(OS_MACOSX)
typedef NSFont* NativeFont;
typedef NSView* NativeView;
typedef NSWindow* NativeWindow;
typedef NSTextField* NativeEditView;
typedef CGContext* NativeDrawingContext;
typedef void* NativeCursor;
typedef void* NativeMenu;
#elif defined(USE_X11)
typedef PangoFontDescription* NativeFont;
typedef GtkWidget* NativeView;
typedef GtkWindow* NativeWindow;
typedef GtkWidget* NativeEditView;
typedef cairo_t* NativeDrawingContext;
typedef GdkCursor* NativeCursor;
typedef GtkWidget* NativeMenu;
typedef GdkRegion* NativeRegion;
#endif

#if defined(OS_MACOSX)
typedef NSImage NativeImageType;
#elif defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
typedef GdkPixbuf NativeImageType;
#else
typedef SkBitmap NativeImageType;
#endif
typedef NativeImageType* NativeImage;

// Note: for test_shell we're packing a pointer into the NativeViewId. So, if
// you make it a type which is smaller than a pointer, you have to fix
// test_shell.
//
// See comment at the top of the file for usage.
typedef intptr_t NativeViewId;

#if defined(OS_WIN)
// Convert a NativeViewId to a NativeView.
//
// On Windows, we pass an HWND into the renderer. As stated above, the renderer
// should not be performing operations on the view.
static inline NativeView NativeViewFromId(NativeViewId id) {
  return reinterpret_cast<NativeView>(id);
}
#define NativeViewFromIdInBrowser(x) NativeViewFromId(x)
#elif defined(OS_POSIX)
// On Mac and Linux, a NativeView is a pointer to an object, and is useless
// outside the process in which it was created. NativeViewFromId should only be
// used inside the appropriate platform ifdef outside of the browser.
// (NativeViewFromIdInBrowser can be used everywhere in the browser.) If your
// cross-platform design involves a call to NativeViewFromId from outside the
// browser it will never work on Mac or Linux and is fundamentally broken.

// Please do not call this from outside the browser. It won't work; the name
// should give you a subtle hint.
static inline NativeView NativeViewFromIdInBrowser(NativeViewId id) {
  return reinterpret_cast<NativeView>(id);
}
#endif  // defined(OS_POSIX)

// Convert a NativeView to a NativeViewId.  See the comments at the top of
// this file.
#if defined(OS_WIN) || defined(OS_MACOSX)
static inline NativeViewId IdFromNativeView(NativeView view) {
  return reinterpret_cast<NativeViewId>(view);
}
#elif defined(USE_X11)
// Not inlined because it involves pulling too many headers.
NativeViewId IdFromNativeView(NativeView view);
#endif  // defined(USE_X11)


// PluginWindowHandle is an abstraction wrapping "the types of windows
// used by NPAPI plugins".  On Windows it's an HWND, on X it's an X
// window id.
#if defined(OS_WIN)
  typedef HWND PluginWindowHandle;
  const PluginWindowHandle kNullPluginWindow = NULL;
#elif defined(USE_X11)
  typedef unsigned long PluginWindowHandle;
  const PluginWindowHandle kNullPluginWindow = 0;
#else
  // On OS X we don't have windowed plugins.
  // We use a NULL/0 PluginWindowHandle in shared code to indicate there
  // is no window present, so mirror that behavior here.
  //
  // The GPU plugin is currently an exception to this rule. As of this
  // writing it uses some NPAPI infrastructure, and minimally we need
  // to identify the plugin instance via this window handle. When the
  // GPU plugin becomes a full-on GPU process, this typedef can be
  // returned to a bool. For now we use a type large enough to hold a
  // pointer on 64-bit architectures in case we need this capability.
  typedef uint64 PluginWindowHandle;
  const PluginWindowHandle kNullPluginWindow = 0;
#endif

// AcceleratedWidget provides a surface to compositors to paint pixels.
#if defined(OS_WIN)
typedef HWND AcceleratedWidget;
const AcceleratedWidget kNullAcceleratedWidget = NULL;
#elif defined(USE_X11)
typedef unsigned long AcceleratedWidget;
const AcceleratedWidget kNullAcceleratedWidget = 0;
#else
typedef void* AcceleratedWidget;
const AcceleratedWidget kNullAcceleratedWidget = NULL;
#endif

}  // namespace gfx

#endif  // UI_GFX_NATIVE_WIDGET_TYPES_H_
