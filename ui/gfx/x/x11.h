// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header file replaces includes of X11 system includes while
// preventing them from redefining and making a mess of commonly used
// keywords like "None" and "Status". Instead those are placed inside
// an X11 namespace where they will not clash with other code.

#ifndef UI_GFX_X_X11
#define UI_GFX_X_X11

extern "C" {
// Xlib.h defines base types so it must be included before the less
// central X11 headers can be included.
#include <X11/Xlib.h>

// TODO(bratell): ui/gl headers sometimes indirectly include Xlib.h
// and then undef Bool. If that has happened, then the include above
// has no effect and Bool will be missing for the rest of the
// includes. This will be fixed when ui/gl uses ui/gfx/x/x11.h (this
// file) but it's not 100% trivial.
#define Bool int

// And the rest so that nobody needs to include them manually...
#include <X11/Xatom.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xregion.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/scrnsaver.h>
#include <X11/extensions/shape.h>
#include <X11/keysym.h>

// These commonly used names are undefined and if necessary recreated
// in the x11 namespace below. This is the main purpose of this header
// file.

// Not redefining common words is extra important for jumbo builds
// where cc files are merged. Those merged filed get to see many more
// headers than initially expected, including system headers like
// those from X11.

#undef None           // Defined by X11/X.h to 0L
#undef Status         // Defined by X11/Xlib.h to int
#undef True           // Defined by X11/Xlib.h to 1
#undef False          // Defined by X11/Xlib.h to 0
#undef Bool           // Defined by X11/Xlib.h to int
#undef RootWindow     // Defined by X11/Xlib.h
#undef CurrentTime    // Defined by X11/X.h to 0L
#undef Success        // Defined by X11/X.h to 0
#undef DestroyAll     // Defined by X11/X.h to 0
#undef COUNT          // Defined by X11/extensions/XI.h to 0
#undef CREATE         // Defined by X11/extensions/XI.h to 1
#undef DeviceAdded    // Defined by X11/extensions/XI.h to 0
#undef DeviceRemoved  // Defined by X11/extensions/XI.h to 1
}

// The x11 namespace allows to scope X11 constants and types that
// would be problematic at the default preprocessor level.
namespace x11 {
static const long None = 0L;
static const long CurrentTime = 0L;
static const int False = 0;
static const int True = 1;
static const int Success = 0;
typedef int Status;
}  // namespace x11

#endif  // UI_GFX_X_X11
