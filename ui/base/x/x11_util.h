// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_UTIL_H_
#define UI_BASE_X_X11_UTIL_H_
#pragma once

// This file declares utility functions for X11 (Linux only).
//
// These functions do not require the Xlib headers to be included (which is why
// we use a void* for Visual*). The Xlib headers are highly polluting so we try
// hard to limit their spread into the rest of the code.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "ui/base/events.h"
#include "ui/base/ui_export.h"

typedef unsigned long Atom;
typedef unsigned long XID;
typedef unsigned long XSharedMemoryId;  // ShmSeg in the X headers.
typedef struct _XDisplay Display;
typedef unsigned long Cursor;
typedef struct _XcursorImage XcursorImage;

#if defined(TOOLKIT_GTK)
typedef struct _GdkDrawable GdkWindow;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;
#endif

namespace gfx {
class Rect;
}

namespace ui {

// These functions use the GDK default display and this /must/ be called from
// the UI thread. Thus, they don't support multiple displays.

// These functions cache their results ---------------------------------

// Check if there's an open connection to an X server.
UI_EXPORT bool XDisplayExists();
// Return an X11 connection for the current, primary display.

// TODO(oshima|evan): This assume there is one display and dosn't work
// undef mutiple displays/monitor environment. Remove this and change the
// chrome codebase to get the display from window.
UI_EXPORT Display* GetXDisplay();

// X shared memory comes in three flavors:
// 1) No SHM support,
// 2) SHM putimage,
// 3) SHM pixmaps + putimage.
enum SharedMemorySupport {
  SHARED_MEMORY_NONE,
  SHARED_MEMORY_PUTIMAGE,
  SHARED_MEMORY_PIXMAP
};
// Return the shared memory type of our X connection.
UI_EXPORT SharedMemorySupport QuerySharedMemorySupport(Display* dpy);

// Return true iff the display supports Xrender
UI_EXPORT bool QueryRenderSupport(Display* dpy);

// Return the default screen number for the display
int GetDefaultScreen(Display* display);

// TODO(xiyuan): Fix the stale XCursorCache problem per http://crbug.com/102759.
// A special cursor that makes GetXCursor below to clear its XCursorCache.
const int kCursorClearXCursorCache = -1;

// Returns an X11 Cursor, sharable across the process.
// |cursor_shape| is an X font cursor shape, see XCreateFontCursor().
UI_EXPORT ::Cursor GetXCursor(int cursor_shape);

#if defined(USE_AURA)
// Creates a custom X cursor from the image. This takes ownership of image. The
// caller must not free/modify the image. The refcount of the newly created
// cursor is set to 1.
UI_EXPORT ::Cursor CreateReffedCustomXCursor(XcursorImage* image);

// Increases the refcount of the custom cursor.
UI_EXPORT void RefCustomXCursor(::Cursor cursor);

// Decreases the refcount of the custom cursor, and destroys it if it reaches 0.
UI_EXPORT void UnrefCustomXCursor(::Cursor cursor);
#endif

// These functions do not cache their results --------------------------

// Get the X window id for the default root window
UI_EXPORT XID GetX11RootWindow();

// Returns the user's current desktop.
bool GetCurrentDesktop(int* desktop);

#if defined(TOOLKIT_GTK)
// Get the X window id for the given GTK widget.
UI_EXPORT XID GetX11WindowFromGtkWidget(GtkWidget* widget);
XID GetX11WindowFromGdkWindow(GdkWindow* window);

// Get the GtkWindow* wrapping a given XID, if any.
// Returns NULL if there isn't already a GtkWindow* wrapping this XID;
// see gdk_window_foreign_new() etc. to wrap arbitrary XIDs.
UI_EXPORT GtkWindow* GetGtkWindowFromX11Window(XID xid);

// Get a Visual from the given widget. Since we don't include the Xlib
// headers, this is returned as a void*.
UI_EXPORT void* GetVisualFromGtkWidget(GtkWidget* widget);
#endif  // defined(TOOLKIT_GTK)

// Return the number of bits-per-pixel for a pixmap of the given depth
UI_EXPORT int BitsPerPixelForPixmapDepth(Display* display, int depth);

// Returns true if |window| is visible.
UI_EXPORT bool IsWindowVisible(XID window);

// Returns the bounds of |window|.
UI_EXPORT bool GetWindowRect(XID window, gfx::Rect* rect);

// Return true if |window| has any property with |property_name|.
UI_EXPORT bool PropertyExists(XID window, const std::string& property_name);

// Get the value of an int, int array, atom array or string property.  On
// success, true is returned and the value is stored in |value|.
UI_EXPORT bool GetIntProperty(XID window, const std::string& property_name,
                              int* value);
UI_EXPORT bool GetIntArrayProperty(XID window, const std::string& property_name,
                                   std::vector<int>* value);
UI_EXPORT bool GetAtomArrayProperty(XID window,
                                    const std::string& property_name,
                                    std::vector<Atom>* value);
UI_EXPORT bool GetStringProperty(
    XID window, const std::string& property_name, std::string* value);

UI_EXPORT bool SetIntProperty(XID window,
                              const std::string& name,
                              const std::string& type,
                              int value);
UI_EXPORT bool SetIntArrayProperty(XID window,
                                   const std::string& name,
                                   const std::string& type,
                                   const std::vector<int>& value);

// Gets the X atom for default display corresponding to atom_name.
Atom GetAtom(const char* atom_name);

// Get |window|'s parent window, or None if |window| is the root window.
XID GetParentWindow(XID window);

// Walk up |window|'s hierarchy until we find a direct child of |root|.
XID GetHighestAncestorWindow(XID window, XID root);

static const int kAllDesktops = -1;
// Queries the desktop |window| is on, kAllDesktops if sticky. Returns false if
// property not found.
bool GetWindowDesktop(XID window, int* desktop);

// Implementers of this interface receive a notification for every X window of
// the main display.
class EnumerateWindowsDelegate {
 public:
  // |xid| is the X Window ID of the enumerated window.  Return true to stop
  // further iteration.
  virtual bool ShouldStopIterating(XID xid) = 0;

 protected:
  virtual ~EnumerateWindowsDelegate() {}
};

// Enumerates all windows in the current display.  Will recurse into child
// windows up to a depth of |max_depth|.
UI_EXPORT bool EnumerateAllWindows(EnumerateWindowsDelegate* delegate,
                                   int max_depth);

// Returns all children windows of a given window in top-to-bottom stacking
// order.
UI_EXPORT bool GetXWindowStack(XID window, std::vector<XID>* windows);

// Restack a window in relation to one of its siblings.  If |above| is true,
// |window| will be stacked directly above |sibling|; otherwise it will stacked
// directly below it.  Both windows must be immediate children of the same
// window.
void RestackWindow(XID window, XID sibling, bool above);

// Return a handle to a X ShmSeg. |shared_memory_key| is a SysV
// IPC key. The shared memory region must contain 32-bit pixels.
UI_EXPORT XSharedMemoryId AttachSharedMemory(Display* display,
                                             int shared_memory_support);
UI_EXPORT void DetachSharedMemory(Display* display, XSharedMemoryId shmseg);

// Return a handle to an XRender picture where |pixmap| is a handle to a
// pixmap containing Skia ARGB data.
UI_EXPORT XID CreatePictureFromSkiaPixmap(Display* display, XID pixmap);

// Draws ARGB data on the given pixmap using the given GC, converting to the
// server side visual depth as needed.  Destination is assumed to be the same
// dimensions as |data| or larger.  |data| is also assumed to be in row order
// with each line being exactly |width| * 4 bytes long.
UI_EXPORT void PutARGBImage(Display* display,
                            void* visual, int depth,
                            XID pixmap, void* pixmap_gc,
                            const uint8* data,
                            int width, int height);

// Same as above only more general:
// - |data_width| and |data_height| refer to the data image
// - |src_x|, |src_y|, |copy_width| and |copy_height| define source region
// - |dst_x|, |dst_y|, |copy_width| and |copy_height| define destination region
UI_EXPORT void PutARGBImage(Display* display,
                            void* visual, int depth,
                            XID pixmap, void* pixmap_gc,
                            const uint8* data,
                            int data_width, int data_height,
                            int src_x, int src_y,
                            int dst_x, int dst_y,
                            int copy_width, int copy_height);

void FreePicture(Display* display, XID picture);
void FreePixmap(Display* display, XID pixmap);

enum WindowManagerName {
  WM_UNKNOWN,
  WM_BLACKBOX,
  WM_CHROME_OS,
  WM_COMPIZ,
  WM_ENLIGHTENMENT,
  WM_ICE_WM,
  WM_KWIN,
  WM_METACITY,
  WM_MUTTER,
  WM_OPENBOX,
  WM_XFWM4,
};
// Attempts to guess the window maager. Returns WM_UNKNOWN if we can't
// determine it for one reason or another.
UI_EXPORT WindowManagerName GuessWindowManager();

// Change desktop for |window| to the desktop of |destination| window.
UI_EXPORT bool ChangeWindowDesktop(XID window, XID destination);

// Enable the default X error handlers. These will log the error and abort
// the process if called. Use SetX11ErrorHandlers() from x11_util_internal.h
// to set your own error handlers.
UI_EXPORT void SetDefaultX11ErrorHandlers();

// Return true if a given window is in full-screen mode.
UI_EXPORT bool IsX11WindowFullScreen(XID window);

// Return true if event type is MotionNotify.
UI_EXPORT bool IsMotionEvent(XEvent* event);

// Returns the mapped button.
int GetMappedButton(int button);

// Updates button mapping. This is usually called when a MappingNotify event is
// received.
UI_EXPORT void UpdateButtonMap();

// Initializes a XEvent that holds XKeyEvent for testing. Note that ui::EF_
// flags should be passed as |flags|, not the native ones in <X11/X.h>.
UI_EXPORT void InitXKeyEventForTesting(EventType type,
                                       KeyboardCode key_code,
                                       int flags,
                                       XEvent* event);

// Keeps track of a string returned by an X function (e.g. XGetAtomName) and
// makes sure it's XFree'd.
class UI_EXPORT XScopedString {
 public:
  explicit XScopedString(char* str) : string_(str) { }
  ~XScopedString();

  const char* string() const { return string_; }

 private:
  char* string_;

  DISALLOW_COPY_AND_ASSIGN(XScopedString);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_UTIL_H_
