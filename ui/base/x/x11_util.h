// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

typedef unsigned long Atom;
typedef struct _GdkDrawable GdkWindow;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;
typedef unsigned long XID;
typedef unsigned long XSharedMemoryId;  // ShmSeg in the X headers.
typedef struct _XDisplay Display;

namespace gfx {
class Rect;
}

namespace ui {

// These functions use the GDK default display and this /must/ be called from
// the UI thread. Thus, they don't support multiple displays.

// These functions cache their results ---------------------------------

// Check if there's an open connection to an X server.
bool XDisplayExists();
// Return an X11 connection for the current, primary display.
Display* GetXDisplay();

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
SharedMemorySupport QuerySharedMemorySupport(Display* dpy);

// Return true iff the display supports Xrender
bool QueryRenderSupport(Display* dpy);

// Return the default screen number for the display
int GetDefaultScreen(Display* display);

// These functions do not cache their results --------------------------

// Get the X window id for the default root window
XID GetX11RootWindow();
// Returns the user's current desktop.
bool GetCurrentDesktop(int* desktop);
// Get the X window id for the given GTK widget.
XID GetX11WindowFromGtkWidget(GtkWidget* widget);
XID GetX11WindowFromGdkWindow(GdkWindow* window);
// Get a Visual from the given widget. Since we don't include the Xlib
// headers, this is returned as a void*.
void* GetVisualFromGtkWidget(GtkWidget* widget);
// Return the number of bits-per-pixel for a pixmap of the given depth
int BitsPerPixelForPixmapDepth(Display* display, int depth);
// Returns true if |window| is visible.
bool IsWindowVisible(XID window);
// Returns the bounds of |window|.
bool GetWindowRect(XID window, gfx::Rect* rect);
// Return true if |window| has any property with |property_name|.
bool PropertyExists(XID window, const std::string& property_name);
// Get the value of an int, int array, atom array or string property.  On
// success, true is returned and the value is stored in |value|.
bool GetIntProperty(XID window, const std::string& property_name, int* value);
bool GetIntArrayProperty(XID window, const std::string& property_name,
                         std::vector<int>* value);
bool GetAtomArrayProperty(XID window, const std::string& property_name,
                          std::vector<Atom>* value);
bool GetStringProperty(
    XID window, const std::string& property_name, std::string* value);

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
bool EnumerateAllWindows(EnumerateWindowsDelegate* delegate, int max_depth);

// Returns all children windows of a given window in top-to-bottom stacking
// order.
bool GetXWindowStack(XID window, std::vector<XID>* windows);

// Restack a window in relation to one of its siblings.  If |above| is true,
// |window| will be stacked directly above |sibling|; otherwise it will stacked
// directly below it.  Both windows must be immediate children of the same
// window.
void RestackWindow(XID window, XID sibling, bool above);

// Return a handle to a X ShmSeg. |shared_memory_key| is a SysV
// IPC key. The shared memory region must contain 32-bit pixels.
XSharedMemoryId AttachSharedMemory(Display* display, int shared_memory_support);
void DetachSharedMemory(Display* display, XSharedMemoryId shmseg);

// Return a handle to an XRender picture where |pixmap| is a handle to a
// pixmap containing Skia ARGB data.
XID CreatePictureFromSkiaPixmap(Display* display, XID pixmap);

// Draws ARGB data on the given pixmap using the given GC, converting to the
// server side visual depth as needed.  Destination is assumed to be the same
// dimensions as |data| or larger.  |data| is also assumed to be in row order
// with each line being exactly |width| * 4 bytes long.
void PutARGBImage(Display* display, void* visual, int depth, XID pixmap,
                  void* pixmap_gc, const uint8* data, int width, int height);

void FreePicture(Display* display, XID picture);
void FreePixmap(Display* display, XID pixmap);

// These functions are for performing X opertions outside of the UI thread.

// Return the Display for the secondary X connection. We keep a second
// connection around for making X requests outside of the UI thread.
// This function may only be called from the BACKGROUND_X11 thread.
Display* GetSecondaryDisplay();

// Since one cannot include both WebKit header and Xlib headers in the same
// file (due to collisions), we wrap all the Xlib functions that we need here.
// These functions must be called on the BACKGROUND_X11 thread since they
// reference GetSecondaryDisplay().

// Get the position of the given window in screen coordinates as well as its
// current size.
bool GetWindowGeometry(int* x, int* y, unsigned* width, unsigned* height,
                       XID window);

// Find the immediate parent of an X window.
//
// parent_window: (output) the parent window of |window|, or 0.
// parent_is_root: (output) true iff the parent of |window| is the root window.
bool GetWindowParent(XID* parent_window, bool* parent_is_root, XID window);

// Get the window manager name.
bool GetWindowManagerName(std::string* name);

// Grabs a snapshot of the designated window and stores a PNG representation
// into a byte vector.
void GrabWindowSnapshot(GtkWindow* gdk_window,
                        std::vector<unsigned char>* png_representation);

// Change desktop for |window| to the desktop of |destination| window.
bool ChangeWindowDesktop(XID window, XID destination);

// Enable the default X error handlers. These will log the error and abort
// the process if called. Use SetX11ErrorHandlers() from x11_util_internal.h
// to set your own error handlers.
void SetDefaultX11ErrorHandlers();

// Return true if a given window is in full-screen mode.
bool IsX11WindowFullScreen(XID window);

}  // namespace ui

#endif  // UI_BASE_X_X11_UTIL_H_
