// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_WIDGET_TYPES_H_
#define UI_GFX_NATIVE_WIDGET_TYPES_H_
#pragma once

#include "base/basictypes.h"
#include "ui/base/ui_export.h"

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

#if defined(USE_AURA)
class SkRegion;
namespace aura {
class Event;
class Window;
}
#endif  // defined(USE_AURA)

#if defined(OS_WIN)
#include <windows.h>  // NOLINT
typedef struct HFONT__* HFONT;
struct IAccessible;
#elif defined(OS_MACOSX)
struct CGContext;
#ifdef __OBJC__
@class NSEvent;
@class NSFont;
@class NSImage;
@class NSView;
@class NSWindow;
@class NSTextField;
#else
class NSEvent;
class NSFont;
class NSImage;
class NSView;
class NSWindow;
class NSTextField;
#endif  // __OBJC__
#elif defined(OS_POSIX)
typedef struct _PangoFontDescription PangoFontDescription;
typedef struct _cairo cairo_t;
#endif

#if defined(USE_WAYLAND)
typedef struct _GdkPixbuf GdkPixbuf;
struct wl_egl_window;

namespace ui {
class WaylandWindow;
}

typedef struct _GdkRegion GdkRegion;
#elif defined(TOOLKIT_USES_GTK)
typedef struct _GdkCursor GdkCursor;
typedef union _GdkEvent GdkEvent;
typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GdkRegion GdkRegion;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;
#elif defined(OS_ANDROID)
class ChromeView;
#endif
class SkBitmap;

namespace gfx {

#if defined(USE_AURA)
// See ui/aura/cursor.h for values.
typedef int NativeCursor;
typedef aura::Window* NativeView;
typedef aura::Window* NativeWindow;
typedef SkRegion* NativeRegion;
typedef aura::Event* NativeEvent;
#elif defined(OS_WIN)
typedef HCURSOR NativeCursor;
typedef HWND NativeView;
typedef HWND NativeWindow;
typedef HRGN NativeRegion;
typedef MSG NativeEvent;
#elif defined(OS_MACOSX)
typedef void* NativeCursor;
typedef NSView* NativeView;
typedef NSWindow* NativeWindow;
typedef NSEvent* NativeEvent;
#elif defined(USE_WAYLAND)
typedef void* NativeCursor;
typedef ui::WaylandWindow* NativeView;
typedef ui::WaylandWindow* NativeWindow;
// TODO(dnicoara) This should be replaced with a cairo region or maybe
// a Wayland specific region
typedef GdkRegion* NativeRegion;
typedef void* NativeEvent;
#elif defined(TOOLKIT_USES_GTK)
typedef GdkCursor* NativeCursor;
typedef GtkWidget* NativeView;
typedef GtkWindow* NativeWindow;
typedef GdkRegion* NativeRegion;
typedef GdkEvent* NativeEvent;
#elif defined(OS_ANDROID)
typedef void* NativeCursor;
typedef ChromeView* NativeView;
typedef ChromeView* NativeWindow;
typedef void* NativeRegion;
typedef void* NativeEvent;
#endif

#if defined(OS_WIN)
typedef HFONT NativeFont;
typedef HWND NativeEditView;
typedef HDC NativeDrawingContext;
typedef HMENU NativeMenu;
typedef IAccessible* NativeViewAccessible;
#elif defined(OS_MACOSX)
typedef NSFont* NativeFont;
typedef NSTextField* NativeEditView;
typedef CGContext* NativeDrawingContext;
typedef void* NativeMenu;
typedef void* NativeViewAccessible;
#elif defined(USE_WAYLAND)
typedef PangoFontDescription* NativeFont;
typedef void* NativeEditView;
typedef cairo_t* NativeDrawingContext;
typedef void* NativeMenu;
typedef void* NativeViewAccessible;
#elif defined(TOOLKIT_USES_GTK)
typedef PangoFontDescription* NativeFont;
typedef GtkWidget* NativeEditView;
typedef cairo_t* NativeDrawingContext;
typedef GtkWidget* NativeMenu;
typedef void* NativeViewAccessible;
#elif defined(USE_AURA)
typedef PangoFontDescription* NativeFont;
typedef void* NativeEditView;
typedef cairo_t* NativeDrawingContext;
typedef void* NativeMenu;
typedef void* NativeViewAccessible;
#elif defined(OS_ANDROID)
typedef void* NativeFont;
typedef void* NativeEditView;
typedef void* NativeDrawingContext;
typedef void* NativeMenu;
typedef void* NativeViewAccessible;
#endif

// A constant value to indicate that gfx::NativeCursor refers to no cursor.
const gfx::NativeCursor kNullCursor = static_cast<gfx::NativeCursor>(NULL);

#if defined(OS_MACOSX)
typedef NSImage NativeImageType;
#elif defined(TOOLKIT_GTK)
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

#if defined(OS_WIN) && !defined(USE_AURA)
// Convert a NativeViewId to a NativeView.
//
// On Windows, we pass an HWND into the renderer. As stated above, the renderer
// should not be performing operations on the view.
static inline NativeView NativeViewFromId(NativeViewId id) {
  return reinterpret_cast<NativeView>(id);
}
#define NativeViewFromIdInBrowser(x) NativeViewFromId(x)
#elif defined(OS_POSIX) || defined(USE_AURA)
// On Mac, Linux and USE_AURA, a NativeView is a pointer to an object, and is
// useless outside the process in which it was created. NativeViewFromId should
// only be used inside the appropriate platform ifdef outside of the browser.
// (NativeViewFromIdInBrowser can be used everywhere in the browser.) If your
// cross-platform design involves a call to NativeViewFromId from outside the
// browser it will never work on Mac or Linux and is fundamentally broken.

// Please do not call this from outside the browser. It won't work; the name
// should give you a subtle hint.
static inline NativeView NativeViewFromIdInBrowser(NativeViewId id) {
  return reinterpret_cast<NativeView>(id);
}
#endif  // defined(OS_POSIX)

// PluginWindowHandle is an abstraction wrapping "the types of windows
// used by NPAPI plugins".  On Windows it's an HWND, on X it's an X
// window id.
#if defined(OS_WIN)
  typedef HWND PluginWindowHandle;
  const PluginWindowHandle kNullPluginWindow = NULL;
#elif defined(USE_WAYLAND)
  typedef struct wl_egl_window* PluginWindowHandle;
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
#elif defined(USE_WAYLAND)
typedef struct wl_egl_window* AcceleratedWidget;
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
