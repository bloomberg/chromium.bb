// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_NATIVE_TYPES_H_
#define VIEWS_NATIVE_TYPES_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

#if defined(OS_LINUX)
typedef union _GdkEvent GdkEvent;
#endif
#if defined(USE_X11)
typedef union _XEvent XEvent;
#endif
#if defined(USE_WAYLAND)
namespace ui {
union WaylandEvent;
}
#endif

#if defined(USE_AURA)
namespace aura {
class Event;
}
#endif

namespace views {

// A note about NativeEvent and NativeEvent2.
// 1. Eventually TOOLKIT_VIEWS will converge on using XEvent as we remove
//    Gtk/Gdk from the stack.
// 2. TOUCH_UI needs XEvents now for certain event types.
// 3. TOUCH_UI also needs GdkEvents for certain event types.
//
//    => NativeEvent and NativeEvent2.
//
// Once we replace usage of Gtk/Gdk types with Xlib types across the board in
// views, we can remove NativeEvent2 and typedef XEvent* to NativeEvent. The
// world will then be beautiful(ish).

#if defined(USE_AURA)
typedef aura::Event* NativeEvent;
#elif defined(OS_WIN)
typedef MSG NativeEvent;
#elif defined(OS_LINUX)

#if defined(USE_WAYLAND)
typedef ui::WaylandEvent* NativeEvent;
#else
typedef GdkEvent* NativeEvent;
#endif

#endif

#if defined(USE_X11)
typedef XEvent* NativeEvent2;
#else
typedef void* NativeEvent2;
#endif

}  // namespace views

#endif  // VIEWS_NATIVE_TYPES_H_

