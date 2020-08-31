// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines utility functions for X11 (Linux only). This code has been
// ported from XCB since we can't use XCB on Ubuntu while its 32-bit support
// remains woefully incomplete.

#include "ui/base/x/x11_util.h"

#include <ctype.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <bitset>
#include <limits>
#include <list>
#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local_storage.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/base/x/x11_menu_list.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/gfx/x/xproto_util.h"

#if defined(OS_FREEBSD)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

namespace ui {

class TLSDestructionCheckerForX11 {
 public:
  static bool HasBeenDestroyed() {
    return base::ThreadLocalStorage::HasBeenDestroyed();
  }
};

namespace {

// Constants that are part of EWMH.
constexpr int kNetWMStateAdd = 1;
constexpr int kNetWMStateRemove = 0;

// Length in 32-bit multiples of the data to be retrieved for
// XGetWindowProperty.
constexpr int kLongLength = 0x1FFFFFFF; /* MAXINT32 / 4 */

int DefaultX11ErrorHandler(XDisplay* d, XErrorEvent* e) {
  // This callback can be invoked by drivers very late in thread destruction,
  // when Chrome TLS is no longer usable. https://crbug.com/849225.
  if (TLSDestructionCheckerForX11::HasBeenDestroyed())
    return 0;

  if (base::MessageLoopCurrent::Get()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&x11::LogErrorEventDescription, *e));
  } else {
    LOG(ERROR) << "X error received: "
               << "serial " << e->serial << ", "
               << "error_code " << static_cast<int>(e->error_code) << ", "
               << "request_code " << static_cast<int>(e->request_code) << ", "
               << "minor_code " << static_cast<int>(e->minor_code);
  }
  return 0;
}

int DefaultX11IOErrorHandler(XDisplay* d) {
  // If there's an IO error it likely means the X server has gone away
  LOG(ERROR) << "X IO error received (X server probably went away)";
  _exit(1);
}

template <typename T>
bool GetProperty(XID window, const std::string& property_name, T* value) {
  static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4, "");
  auto response = x11::Connection::Get()
                      ->GetProperty({
                          .window = static_cast<x11::Window>(window),
                          .property = static_cast<x11::Atom>(
                              gfx::GetAtom(property_name.c_str())),
                          .long_length = std::max<uint32_t>(1, sizeof(T) / 4),
                      })
                      .Sync();
  if (!response || response->format != 8 * sizeof(T) ||
      response->value.size() != sizeof(T)) {
    return false;
  }

  DCHECK_EQ(response->format / 8 * response->value_len, response->value.size());
  memcpy(value, response->value.data(), sizeof(T));
  return true;
}

template <typename T>
bool GetArrayProperty(XID window,
                      const std::string& property_name,
                      std::vector<T>* value) {
  static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4, "");
  auto response = x11::Connection::Get()
                      ->GetProperty({
                          .window = static_cast<x11::Window>(window),
                          .property = static_cast<x11::Atom>(
                              gfx::GetAtom(property_name.c_str())),
                          .long_length = std::numeric_limits<uint32_t>::max(),
                      })
                      .Sync();
  if (!response || response->format != 8 * sizeof(T))
    return false;

  DCHECK_EQ(response->format / 8 * response->value_len, response->value.size());
  value->resize(response->value_len);
  memcpy(value->data(), response->value.data(), response->value.size());
  return true;
}

bool SupportsEWMH() {
  static bool supports_ewmh = false;
  static bool supports_ewmh_cached = false;
  if (!supports_ewmh_cached) {
    supports_ewmh_cached = true;

    int wm_window = 0u;
    if (!GetIntProperty(GetX11RootWindow(), "_NET_SUPPORTING_WM_CHECK",
                        &wm_window)) {
      supports_ewmh = false;
      return false;
    }

    // It's possible that a window manager started earlier in this X session
    // left a stale _NET_SUPPORTING_WM_CHECK property when it was replaced by a
    // non-EWMH window manager, so we trap errors in the following requests to
    // avoid crashes (issue 23860).

    // EWMH requires the supporting-WM window to also have a
    // _NET_SUPPORTING_WM_CHECK property pointing to itself (to avoid a stale
    // property referencing an ID that's been recycled for another window), so
    // we check that too.
    gfx::X11ErrorTracker err_tracker;
    int wm_window_property = 0;
    bool result = GetIntProperty(wm_window, "_NET_SUPPORTING_WM_CHECK",
                                 &wm_window_property);
    supports_ewmh = !err_tracker.FoundNewError() && result &&
                    wm_window_property == wm_window;
  }

  return supports_ewmh;
}

bool GetWindowManagerName(std::string* wm_name) {
  DCHECK(wm_name);
  if (!SupportsEWMH())
    return false;

  int wm_window = 0;
  if (!GetIntProperty(GetX11RootWindow(), "_NET_SUPPORTING_WM_CHECK",
                      &wm_window)) {
    return false;
  }

  gfx::X11ErrorTracker err_tracker;
  bool result =
      GetStringProperty(static_cast<XID>(wm_window), "_NET_WM_NAME", wm_name);
  return !err_tracker.FoundNewError() && result;
}

unsigned int GetMaxCursorSize() {
  // Although XQueryBestCursor() takes unsigned ints, the width and height will
  // be sent over the wire as 16 bit integers.
  constexpr unsigned int kQuerySize = std::numeric_limits<uint16_t>::max();
  XDisplay* display = gfx::GetXDisplay();
  unsigned int width = 0;
  unsigned int height = 0;
  XQueryBestCursor(display, DefaultRootWindow(display), kQuerySize, kQuerySize,
                   &width, &height);
  unsigned int min_dimension = std::min(width, height);
  // libXcursor defines MAX_BITMAP_CURSOR_SIZE to 64 in src/xcursorint.h, so use
  // this as a fallback in case the X server returns zero size, which can happen
  // on some buggy implementations of XWayland/XMir.
  return min_dimension > 0 ? min_dimension : 64;
}

// A process wide singleton cache for custom X cursors.
class XCustomCursorCache {
 public:
  static XCustomCursorCache* GetInstance() {
    return base::Singleton<XCustomCursorCache>::get();
  }

  ::Cursor InstallCustomCursor(XcursorImage* image) {
    XCustomCursor* custom_cursor = new XCustomCursor(image);
    ::Cursor xcursor = custom_cursor->cursor();
    cache_[xcursor] = custom_cursor;
    return xcursor;
  }

  void Ref(::Cursor cursor) { cache_[cursor]->Ref(); }

  void Unref(::Cursor cursor) {
    if (cache_[cursor]->Unref())
      cache_.erase(cursor);
  }

  void Clear() { cache_.clear(); }

  const XcursorImage* GetXcursorImage(::Cursor cursor) const {
    return cache_.find(cursor)->second->image();
  }

 private:
  friend struct base::DefaultSingletonTraits<XCustomCursorCache>;

  class XCustomCursor {
   public:
    // This takes ownership of the image.
    explicit XCustomCursor(XcursorImage* image) : image_(image), ref_(1) {
      cursor_ = XcursorImageLoadCursor(gfx::GetXDisplay(), image);
    }

    ~XCustomCursor() {
      XcursorImageDestroy(image_);
      XFreeCursor(gfx::GetXDisplay(), cursor_);
    }

    ::Cursor cursor() const { return cursor_; }

    void Ref() { ++ref_; }

    // Returns true if the cursor was destroyed because of the unref.
    bool Unref() {
      if (--ref_ == 0) {
        delete this;
        return true;
      }
      return false;
    }

    const XcursorImage* image() const { return image_; }

   private:
    XcursorImage* image_;
    int ref_;
    ::Cursor cursor_;

    DISALLOW_COPY_AND_ASSIGN(XCustomCursor);
  };

  XCustomCursorCache() = default;
  ~XCustomCursorCache() { Clear(); }

  std::map<::Cursor, XCustomCursor*> cache_;
  DISALLOW_COPY_AND_ASSIGN(XCustomCursorCache);
};

// Converts a SKBitmap to unpremul alpha.
SkBitmap ConvertSkBitmapToUnpremul(const SkBitmap& bitmap) {
  DCHECK_NE(bitmap.alphaType(), kUnpremul_SkAlphaType);

  SkImageInfo image_info = SkImageInfo::MakeN32(bitmap.width(), bitmap.height(),
                                                kUnpremul_SkAlphaType);
  SkBitmap converted_bitmap;
  converted_bitmap.allocPixels(image_info);
  bitmap.readPixels(image_info, converted_bitmap.getPixels(),
                    image_info.minRowBytes(), 0, 0);

  return converted_bitmap;
}

}  // namespace

bool IsXInput2Available() {
  return DeviceDataManagerX11::GetInstance()->IsXInput2Available();
}

bool QueryRenderSupport(Display* dpy) {
  int dummy;
  // We don't care about the version of Xrender since all the features which
  // we use are included in every version.
  static bool render_supported = XRenderQueryExtension(dpy, &dummy, &dummy);
  return render_supported;
}

bool QueryShmSupport() {
  int major;
  int minor;
  x11::Bool pixmaps;
  static bool supported =
      XShmQueryVersion(gfx::GetXDisplay(), &major, &minor, &pixmaps);
  return supported;
}

int ShmEventBase() {
  static int event_base = XShmGetEventBase(gfx::GetXDisplay());
  return event_base;
}

::Cursor CreateReffedCustomXCursor(XcursorImage* image) {
  return XCustomCursorCache::GetInstance()->InstallCustomCursor(image);
}

void RefCustomXCursor(::Cursor cursor) {
  XCustomCursorCache::GetInstance()->Ref(cursor);
}

void UnrefCustomXCursor(::Cursor cursor) {
  XCustomCursorCache::GetInstance()->Unref(cursor);
}

XcursorImage* SkBitmapToXcursorImage(const SkBitmap& cursor_image,
                                     const gfx::Point& hotspot) {
  // TODO(crbug.com/596782): It is possible for cursor_image to be zeroed out
  // at this point, which leads to benign debug errors. Once this is fixed, we
  // should  DCHECK_EQ(cursor_image.colorType(), kN32_SkColorType).

  // X11 expects bitmap with unpremul alpha. If bitmap is premul then convert,
  // otherwise semi-transparent parts of cursor will look strange.
  const SkBitmap converted = (cursor_image.alphaType() != kUnpremul_SkAlphaType)
                                 ? ConvertSkBitmapToUnpremul(cursor_image)
                                 : cursor_image;

  gfx::Point hotspot_point = hotspot;
  SkBitmap scaled;

  // X11 seems to have issues with cursors when images get larger than 64
  // pixels. So rescale the image if necessary.
  static const float kMaxPixel = GetMaxCursorSize();
  bool needs_scale = false;
  if (converted.width() > kMaxPixel || converted.height() > kMaxPixel) {
    float scale = 1.f;
    if (converted.width() > converted.height())
      scale = kMaxPixel / converted.width();
    else
      scale = kMaxPixel / converted.height();

    scaled = skia::ImageOperations::Resize(
        converted, skia::ImageOperations::RESIZE_BETTER,
        static_cast<int>(converted.width() * scale),
        static_cast<int>(converted.height() * scale));
    hotspot_point = gfx::ScaleToFlooredPoint(hotspot, scale);
    needs_scale = true;
  }

  const SkBitmap& bitmap = needs_scale ? scaled : converted;
  XcursorImage* image = XcursorImageCreate(bitmap.width(), bitmap.height());
  image->xhot = std::min(bitmap.width() - 1, hotspot_point.x());
  image->yhot = std::min(bitmap.height() - 1, hotspot_point.y());

  if (bitmap.width() && bitmap.height()) {
    // The |bitmap| contains ARGB image, so just copy it.
    memcpy(image->pixels, bitmap.getPixels(),
           bitmap.width() * bitmap.height() * 4);
  }

  return image;
}

int CoalescePendingMotionEvents(const XEvent* xev, XEvent* last_event) {
  DCHECK(xev->type == MotionNotify || xev->type == GenericEvent);
  XDisplay* display = xev->xany.display;
  XEvent next_event;
  bool is_motion = false;
  int num_coalesced = 0;

  if (xev->type == MotionNotify) {
    is_motion = true;
    while (XPending(display)) {
      XPeekEvent(xev->xany.display, &next_event);
      // Discard all but the most recent motion event that targets the same
      // window with unchanged state.
      if (next_event.type == MotionNotify &&
          next_event.xmotion.window == xev->xmotion.window &&
          next_event.xmotion.subwindow == xev->xmotion.subwindow &&
          next_event.xmotion.state == xev->xmotion.state) {
        XNextEvent(xev->xany.display, last_event);
      } else {
        break;
      }
    }
  } else {
    int event_type = xev->xgeneric.evtype;
    XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
    DCHECK(event_type == XI_Motion || event_type == XI_TouchUpdate);
    is_motion = event_type == XI_Motion;

    while (XPending(display)) {
      XPeekEvent(display, &next_event);

      // If we can't get the cookie, abort the check.
      if (!XGetEventData(next_event.xgeneric.display, &next_event.xcookie))
        return num_coalesced;

      // If this isn't from a valid device, throw the event away, as
      // that's what the message pump would do. Device events come in pairs
      // with one from the master and one from the slave so there will
      // always be at least one pending.
      if (!ui::TouchFactory::GetInstance()->ShouldProcessXI2Event(
              &next_event)) {
        XFreeEventData(display, &next_event.xcookie);
        XNextEvent(display, &next_event);
        continue;
      }

      if (next_event.type == GenericEvent &&
          next_event.xgeneric.evtype == event_type &&
          !ui::DeviceDataManagerX11::GetInstance()->IsCMTGestureEvent(
              next_event) &&
          ui::DeviceDataManagerX11::GetInstance()->GetScrollClassEventDetail(
              next_event) == SCROLL_TYPE_NO_SCROLL) {
        XIDeviceEvent* next_xievent =
            static_cast<XIDeviceEvent*>(next_event.xcookie.data);
        // Confirm that the motion event is targeted at the same window
        // and that no buttons or modifiers have changed.
        if (xievent->event == next_xievent->event &&
            xievent->child == next_xievent->child &&
            xievent->detail == next_xievent->detail &&
            xievent->buttons.mask_len == next_xievent->buttons.mask_len &&
            (memcmp(xievent->buttons.mask, next_xievent->buttons.mask,
                    xievent->buttons.mask_len) == 0) &&
            xievent->mods.base == next_xievent->mods.base &&
            xievent->mods.latched == next_xievent->mods.latched &&
            xievent->mods.locked == next_xievent->mods.locked &&
            xievent->mods.effective == next_xievent->mods.effective) {
          XFreeEventData(display, &next_event.xcookie);
          // Free the previous cookie.
          if (num_coalesced > 0)
            XFreeEventData(display, &last_event->xcookie);
          // Get the event and its cookie data.
          XNextEvent(display, last_event);
          XGetEventData(display, &last_event->xcookie);
          ++num_coalesced;
          continue;
        }
      }
      // This isn't an event we want so free its cookie data.
      XFreeEventData(display, &next_event.xcookie);
      break;
    }
  }

  if (is_motion && num_coalesced > 0)
    UMA_HISTOGRAM_COUNTS_10000("Event.CoalescedCount.Mouse", num_coalesced);
  return num_coalesced;
}

void HideHostCursor() {
  static base::NoDestructor<XScopedCursor> invisible_cursor(
      CreateInvisibleCursor(), gfx::GetXDisplay());
  XDefineCursor(gfx::GetXDisplay(), DefaultRootWindow(gfx::GetXDisplay()),
                invisible_cursor->get());
}

::Cursor CreateInvisibleCursor() {
  XDisplay* xdisplay = gfx::GetXDisplay();
  ::Cursor invisible_cursor;
  char nodata[] = {0, 0, 0, 0, 0, 0, 0, 0};
  XColor black;
  black.red = black.green = black.blue = 0;
  Pixmap blank = XCreateBitmapFromData(xdisplay, DefaultRootWindow(xdisplay),
                                       nodata, 8, 8);
  invisible_cursor =
      XCreatePixmapCursor(xdisplay, blank, blank, &black, &black, 0, 0);
  XFreePixmap(xdisplay, blank);
  return invisible_cursor;
}

void SetUseOSWindowFrame(XID window, bool use_os_window_frame) {
  // This data structure represents additional hints that we send to the window
  // manager and has a direct lineage back to Motif, which defined this de facto
  // standard. This struct doesn't seem 64-bit safe though, but it's what GDK
  // does.
  typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
  } MotifWmHints;

  MotifWmHints motif_hints;
  memset(&motif_hints, 0, sizeof(motif_hints));
  // Signals that the reader of the _MOTIF_WM_HINTS property should pay
  // attention to the value of |decorations|.
  motif_hints.flags = (1L << 1);
  motif_hints.decorations = use_os_window_frame ? 1 : 0;

  XAtom hint_atom = gfx::GetAtom("_MOTIF_WM_HINTS");
  XChangeProperty(gfx::GetXDisplay(), window, hint_atom, hint_atom, 32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&motif_hints),
                  sizeof(MotifWmHints) / sizeof(long));
}

bool IsShapeExtensionAvailable() {
  int dummy;
  static bool is_shape_available =
      XShapeQueryExtension(gfx::GetXDisplay(), &dummy, &dummy);
  return is_shape_available;
}

XID GetX11RootWindow() {
  return DefaultRootWindow(gfx::GetXDisplay());
}

bool GetCurrentDesktop(int* desktop) {
  return GetIntProperty(GetX11RootWindow(), "_NET_CURRENT_DESKTOP", desktop);
}

void SetHideTitlebarWhenMaximizedProperty(XID window,
                                          HideTitlebarWhenMaximized property) {
  // XChangeProperty() expects "hide" to be long.
  unsigned long hide = property;
  XChangeProperty(gfx::GetXDisplay(), window,
                  gfx::GetAtom("_GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED"),
                  XA_CARDINAL,
                  32,  // size in bits
                  PropModeReplace, reinterpret_cast<unsigned char*>(&hide), 1);
}

void ClearX11DefaultRootWindow() {
  XDisplay* display = gfx::GetXDisplay();
  XID root_window = GetX11RootWindow();
  gfx::Rect root_bounds;
  if (!GetOuterWindowBounds(root_window, &root_bounds)) {
    LOG(ERROR) << "Failed to get the bounds of the X11 root window";
    return;
  }

  XGCValues gc_values = {0};
  gc_values.foreground = BlackPixel(display, DefaultScreen(display));
  GC gc = XCreateGC(display, root_window, GCForeground, &gc_values);
  XFillRectangle(display, root_window, gc, root_bounds.x(), root_bounds.y(),
                 root_bounds.width(), root_bounds.height());
  XFreeGC(display, gc);
}

bool IsWindowVisible(XID window) {
  TRACE_EVENT0("ui", "IsWindowVisible");

  XWindowAttributes win_attributes;
  if (!XGetWindowAttributes(gfx::GetXDisplay(), window, &win_attributes))
    return false;
  if (win_attributes.map_state != IsViewable)
    return false;

  // Minimized windows are not visible.
  std::vector<XAtom> wm_states;
  if (GetAtomArrayProperty(window, "_NET_WM_STATE", &wm_states)) {
    XAtom hidden_atom = gfx::GetAtom("_NET_WM_STATE_HIDDEN");
    if (base::Contains(wm_states, hidden_atom))
      return false;
  }

  // Some compositing window managers (notably kwin) do not actually unmap
  // windows on desktop switch, so we also must check the current desktop.
  int window_desktop, current_desktop;
  return (!GetWindowDesktop(window, &window_desktop) ||
          !GetCurrentDesktop(&current_desktop) ||
          window_desktop == kAllDesktops || window_desktop == current_desktop);
}

bool GetInnerWindowBounds(XID window, gfx::Rect* rect) {
  Window root, child;
  int x, y;
  unsigned int width, height;
  unsigned int border_width, depth;

  if (!XGetGeometry(gfx::GetXDisplay(), window, &root, &x, &y, &width, &height,
                    &border_width, &depth))
    return false;

  if (!XTranslateCoordinates(gfx::GetXDisplay(), window, root, 0, 0, &x, &y,
                             &child))
    return false;

  *rect = gfx::Rect(x, y, width, height);

  return true;
}

bool GetWindowExtents(XID window, gfx::Insets* extents) {
  std::vector<int> insets;
  if (!GetIntArrayProperty(window, "_NET_FRAME_EXTENTS", &insets))
    return false;
  if (insets.size() != 4)
    return false;

  int left = insets[0];
  int right = insets[1];
  int top = insets[2];
  int bottom = insets[3];
  extents->Set(-top, -left, -bottom, -right);
  return true;
}

bool GetOuterWindowBounds(XID window, gfx::Rect* rect) {
  if (!GetInnerWindowBounds(window, rect))
    return false;

  gfx::Insets extents;
  if (GetWindowExtents(window, &extents))
    rect->Inset(extents);
  // Not all window managers support _NET_FRAME_EXTENTS so return true even if
  // requesting the property fails.

  return true;
}

bool WindowContainsPoint(XID window, gfx::Point screen_loc) {
  TRACE_EVENT0("ui", "WindowContainsPoint");

  gfx::Rect window_rect;
  if (!GetOuterWindowBounds(window, &window_rect))
    return false;

  if (!window_rect.Contains(screen_loc))
    return false;

  if (!IsShapeExtensionAvailable())
    return true;

  // According to http://www.x.org/releases/X11R7.6/doc/libXext/shapelib.html,
  // if an X display supports the shape extension the bounds of a window are
  // defined as the intersection of the window bounds and the interior
  // rectangles. This means to determine if a point is inside a window for the
  // purpose of input handling we have to check the rectangles in the ShapeInput
  // list.
  // According to http://www.x.org/releases/current/doc/xextproto/shape.html,
  // we need to also respect the ShapeBounding rectangles.
  // The effective input region of a window is defined to be the intersection
  // of the client input region with both the default input region and the
  // client bounding region. Any portion of the client input region that is not
  // included in both the default input region and the client bounding region
  // will not be included in the effective input region on the screen.
  int rectangle_kind[] = {ShapeInput, ShapeBounding};
  for (int kind_index : rectangle_kind) {
    int dummy;
    int shape_rects_size = 0;
    gfx::XScopedPtr<XRectangle[]> shape_rects(XShapeGetRectangles(
        gfx::GetXDisplay(), window, kind_index, &shape_rects_size, &dummy));
    if (!shape_rects) {
      // The shape is empty. This can occur when |window| is minimized.
      DCHECK_EQ(0, shape_rects_size);
      return false;
    }
    bool is_in_shape_rects = false;
    for (int i = 0; i < shape_rects_size; ++i) {
      // The ShapeInput and ShapeBounding rects are to be in window space, so we
      // have to translate by the window_rect's offset to map to screen space.
      const XRectangle& rect = shape_rects[i];
      gfx::Rect shape_rect =
          gfx::Rect(rect.x + window_rect.x(), rect.y + window_rect.y(),
                    rect.width, rect.height);
      if (shape_rect.Contains(screen_loc)) {
        is_in_shape_rects = true;
        break;
      }
    }
    if (!is_in_shape_rects)
      return false;
  }
  return true;
}

bool PropertyExists(XID window, const std::string& property_name) {
  auto response = x11::Connection::Get()
                      ->GetProperty({
                          .window = static_cast<x11::Window>(window),
                          .property = static_cast<x11::Atom>(
                              gfx::GetAtom(property_name.c_str())),
                          .long_length = 1,
                      })
                      .Sync();
  return response && response->format;
}

bool GetRawBytesOfProperty(XID window,
                           XAtom property,
                           scoped_refptr<base::RefCountedMemory>* out_data,
                           size_t* out_data_items,
                           XAtom* out_type) {
  // Retrieve the data from our window.
  unsigned long nitems = 0;
  unsigned long nbytes = 0;
  XAtom prop_type = x11::None;
  int prop_format = 0;
  unsigned char* property_data = nullptr;
  if (XGetWindowProperty(gfx::GetXDisplay(), window, property, 0, kLongLength,
                         x11::False, AnyPropertyType, &prop_type, &prop_format,
                         &nitems, &nbytes, &property_data) != x11::Success) {
    return false;
  }
  gfx::XScopedPtr<unsigned char> scoped_property(property_data);

  if (prop_type == x11::None)
    return false;

  size_t bytes = 0;
  // So even though we should theoretically have nbytes (and we can't
  // pass nullptr there), we need to manually calculate the byte length here
  // because nbytes always returns zero.
  switch (prop_format) {
    case 8:
      bytes = nitems;
      break;
    case 16:
      bytes = sizeof(short) * nitems;
      break;
    case 32:
      bytes = sizeof(long) * nitems;
      break;
    default:
      NOTREACHED();
      break;
  }

  if (out_data)
    *out_data = new XRefcountedMemory(scoped_property.release(), bytes);

  if (out_data_items)
    *out_data_items = nitems;

  if (out_type)
    *out_type = prop_type;

  return true;
}

bool GetIntProperty(XID window, const std::string& property_name, int* value) {
  return GetProperty(window, property_name, value);
}

bool GetXIDProperty(XID window, const std::string& property_name, XID* value) {
  uint32_t xid;
  if (!GetProperty(window, property_name, &xid))
    return false;
  *value = xid;
  return true;
}

bool GetIntArrayProperty(XID window,
                         const std::string& property_name,
                         std::vector<int>* value) {
  return GetArrayProperty(window, property_name, value);
}

bool GetAtomArrayProperty(XID window,
                          const std::string& property_name,
                          std::vector<XAtom>* value) {
  std::vector<uint32_t> value32;
  if (!GetArrayProperty(window, property_name, &value32))
    return false;
  *value = std::vector<XAtom>(value32.begin(), value32.end());
  return true;
}

bool GetStringProperty(XID window,
                       const std::string& property_name,
                       std::string* value) {
  std::vector<char> str;
  if (!GetArrayProperty(window, property_name, &str))
    return false;

  value->assign(str.data(), str.size());
  return true;
}

bool SetIntProperty(XID window,
                    const std::string& name,
                    const std::string& type,
                    int value) {
  std::vector<int> values(1, value);
  return SetIntArrayProperty(window, name, type, values);
}

bool SetIntArrayProperty(XID window,
                         const std::string& name,
                         const std::string& type,
                         const std::vector<int>& value) {
  DCHECK(!value.empty());
  XAtom name_atom = gfx::GetAtom(name.c_str());
  XAtom type_atom = gfx::GetAtom(type.c_str());

  // XChangeProperty() expects values of type 32 to be longs.
  std::unique_ptr<long[]> data(new long[value.size()]);
  for (size_t i = 0; i < value.size(); ++i)
    data[i] = value[i];

  gfx::X11ErrorTracker err_tracker;
  XChangeProperty(gfx::GetXDisplay(), window, name_atom, type_atom,
                  32,  // size in bits of items in 'value'
                  PropModeReplace,
                  reinterpret_cast<const unsigned char*>(data.get()),
                  value.size());  // num items
  return !err_tracker.FoundNewError();
}

bool SetAtomProperty(XID window,
                     const std::string& name,
                     const std::string& type,
                     XAtom value) {
  std::vector<XAtom> values(1, value);
  return SetAtomArrayProperty(window, name, type, values);
}

bool SetAtomArrayProperty(XID window,
                          const std::string& name,
                          const std::string& type,
                          const std::vector<XAtom>& value) {
  DCHECK(!value.empty());
  XAtom name_atom = gfx::GetAtom(name.c_str());
  XAtom type_atom = gfx::GetAtom(type.c_str());

  // XChangeProperty() expects values of type 32 to be longs.
  std::unique_ptr<XAtom[]> data(new XAtom[value.size()]);
  for (size_t i = 0; i < value.size(); ++i)
    data[i] = value[i];

  gfx::X11ErrorTracker err_tracker;
  XChangeProperty(gfx::GetXDisplay(), window, name_atom, type_atom,
                  32,  // size in bits of items in 'value'
                  PropModeReplace,
                  reinterpret_cast<const unsigned char*>(data.get()),
                  value.size());  // num items
  return !err_tracker.FoundNewError();
}

bool SetStringProperty(XID window,
                       XAtom property,
                       XAtom type,
                       const std::string& value) {
  gfx::X11ErrorTracker err_tracker;
  XChangeProperty(
      gfx::GetXDisplay(), window, property, type, 8, PropModeReplace,
      reinterpret_cast<const unsigned char*>(value.c_str()), value.size());
  return !err_tracker.FoundNewError();
}

void SetWindowClassHint(XDisplay* display,
                        XID window,
                        const std::string& res_name,
                        const std::string& res_class) {
  XClassHint class_hints;
  // const_cast is safe because XSetClassHint does not modify the strings.
  // Just to be safe, the res_name and res_class parameters are local copies,
  // not const references.
  class_hints.res_name = const_cast<char*>(res_name.c_str());
  class_hints.res_class = const_cast<char*>(res_class.c_str());
  XSetClassHint(display, window, &class_hints);
}

void SetWindowRole(XDisplay* display, XID window, const std::string& role) {
  if (role.empty()) {
    XDeleteProperty(display, window, gfx::GetAtom("WM_WINDOW_ROLE"));
  } else {
    char* role_c = const_cast<char*>(role.c_str());
    XChangeProperty(display, window, gfx::GetAtom("WM_WINDOW_ROLE"), XA_STRING,
                    8, PropModeReplace,
                    reinterpret_cast<unsigned char*>(role_c), role.size());
  }
}

void SetWMSpecState(XID window, bool enabled, XAtom state1, XAtom state2) {
  XEvent xclient;
  memset(&xclient, 0, sizeof(xclient));
  xclient.type = ClientMessage;
  xclient.xclient.window = window;
  xclient.xclient.message_type = gfx::GetAtom("_NET_WM_STATE");
  // The data should be viewed as a list of longs, because XAtom is a typedef of
  // long.
  xclient.xclient.format = 32;
  xclient.xclient.data.l[0] = enabled ? kNetWMStateAdd : kNetWMStateRemove;
  xclient.xclient.data.l[1] = state1;
  xclient.xclient.data.l[2] = state2;
  xclient.xclient.data.l[3] = 1;
  xclient.xclient.data.l[4] = 0;

  XSendEvent(gfx::GetXDisplay(), GetX11RootWindow(), x11::False,
             SubstructureRedirectMask | SubstructureNotifyMask, &xclient);
}

void DoWMMoveResize(XDisplay* display,
                    XID root_window,
                    XID window,
                    const gfx::Point& location_px,
                    int direction) {
  // This handler is usually sent when the window has the implicit grab.  We
  // need to dump it because what we're about to do is tell the window manager
  // that it's now responsible for moving the window around; it immediately
  // grabs when it receives the event below.
  XUngrabPointer(display, x11::CurrentTime);

  XEvent event;
  memset(&event, 0, sizeof(event));
  event.xclient.type = ClientMessage;
  event.xclient.display = display;
  event.xclient.window = window;
  event.xclient.message_type = gfx::GetAtom("_NET_WM_MOVERESIZE");
  event.xclient.format = 32;
  event.xclient.data.l[0] = location_px.x();
  event.xclient.data.l[1] = location_px.y();
  event.xclient.data.l[2] = direction;

  XSendEvent(display, root_window, x11::False,
             SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

bool HasWMSpecProperty(const base::flat_set<XAtom>& properties, XAtom atom) {
  return properties.find(atom) != properties.end();
}

bool GetCustomFramePrefDefault() {
  // If the window manager doesn't support enough of EWMH to tell us its name,
  // assume that it doesn't want custom frames. For example, _NET_WM_MOVERESIZE
  // is needed for frame-drag-initiated window movement.
  std::string wm_name;
  if (!GetWindowManagerName(&wm_name))
    return false;

  // Also disable custom frames for (at-least-partially-)EWMH-supporting tiling
  // window managers.
  ui::WindowManagerName wm = GuessWindowManager();
  if (wm == WM_AWESOME || wm == WM_I3 || wm == WM_ION3 || wm == WM_MATCHBOX ||
      wm == WM_NOTION || wm == WM_QTILE || wm == WM_RATPOISON ||
      wm == WM_STUMPWM || wm == WM_WMII)
    return false;

  // Handle a few more window managers that don't get along well with custom
  // frames.
  if (wm == WM_ICE_WM || wm == WM_KWIN)
    return false;

  // For everything else, use custom frames.
  return true;
}

bool IsWmTiling(WindowManagerName window_manager) {
  switch (window_manager) {
    case WM_BLACKBOX:
    case WM_COMPIZ:
    case WM_ENLIGHTENMENT:
    case WM_FLUXBOX:
    case WM_ICE_WM:
    case WM_KWIN:
    case WM_MATCHBOX:
    case WM_METACITY:
    case WM_MUFFIN:
    case WM_MUTTER:
    case WM_OPENBOX:
    case WM_XFWM4:
      // Stacking window managers.
      return false;

    case WM_I3:
    case WM_ION3:
    case WM_NOTION:
    case WM_RATPOISON:
    case WM_STUMPWM:
      // Tiling window managers.
      return true;

    case WM_AWESOME:
    case WM_QTILE:
    case WM_XMONAD:
    case WM_WMII:
      // Dynamic (tiling and stacking) window managers.  Assume tiling.
      return true;

    case WM_OTHER:
    case WM_UNNAMED:
      // Unknown.  Assume stacking.
      return false;
  }
}

bool GetWindowDesktop(XID window, int* desktop) {
  return GetIntProperty(window, "_NET_WM_DESKTOP", desktop);
}

std::string GetX11ErrorString(XDisplay* display, int err) {
  char buffer[256];
  XGetErrorText(display, err, buffer, base::size(buffer));
  return buffer;
}

// Returns true if |window| is a named window.
bool IsWindowNamed(XID window) {
  XTextProperty prop;
  if (!XGetWMName(gfx::GetXDisplay(), window, &prop) || !prop.value)
    return false;

  XFree(prop.value);
  return true;
}

bool EnumerateChildren(EnumerateWindowsDelegate* delegate,
                       XID window,
                       const int max_depth,
                       int depth) {
  if (depth > max_depth)
    return false;

  std::vector<XID> windows;
  std::vector<XID>::iterator iter;
  if (depth == 0) {
    XMenuList::GetInstance()->InsertMenuWindowXIDs(&windows);
    // Enumerate the menus first.
    for (iter = windows.begin(); iter != windows.end(); iter++) {
      if (delegate->ShouldStopIterating(*iter))
        return true;
    }
    windows.clear();
  }

  XID root, parent, *children;
  unsigned int num_children;
  int status = XQueryTree(gfx::GetXDisplay(), window, &root, &parent, &children,
                          &num_children);
  if (status == 0)
    return false;

  for (int i = static_cast<int>(num_children) - 1; i >= 0; i--)
    windows.push_back(children[i]);

  XFree(children);

  // XQueryTree returns the children of |window| in bottom-to-top order, so
  // reverse-iterate the list to check the windows from top-to-bottom.
  for (iter = windows.begin(); iter != windows.end(); iter++) {
    if (IsWindowNamed(*iter) && delegate->ShouldStopIterating(*iter))
      return true;
  }

  // If we're at this point, we didn't find the window we're looking for at the
  // current level, so we need to recurse to the next level.  We use a second
  // loop because the recursion and call to XQueryTree are expensive and is only
  // needed for a small number of cases.
  if (++depth <= max_depth) {
    for (iter = windows.begin(); iter != windows.end(); iter++) {
      if (EnumerateChildren(delegate, *iter, max_depth, depth))
        return true;
    }
  }

  return false;
}

bool EnumerateAllWindows(EnumerateWindowsDelegate* delegate, int max_depth) {
  XID root = GetX11RootWindow();
  return EnumerateChildren(delegate, root, max_depth, 0);
}

void EnumerateTopLevelWindows(ui::EnumerateWindowsDelegate* delegate) {
  std::vector<XID> stack;
  if (!ui::GetXWindowStack(ui::GetX11RootWindow(), &stack)) {
    // Window Manager doesn't support _NET_CLIENT_LIST_STACKING, so fall back
    // to old school enumeration of all X windows.  Some WMs parent 'top-level'
    // windows in unnamed actual top-level windows (ion WM), so extend the
    // search depth to all children of top-level windows.
    const int kMaxSearchDepth = 1;
    ui::EnumerateAllWindows(delegate, kMaxSearchDepth);
    return;
  }
  XMenuList::GetInstance()->InsertMenuWindowXIDs(&stack);

  std::vector<XID>::iterator iter;
  for (iter = stack.begin(); iter != stack.end(); iter++) {
    if (delegate->ShouldStopIterating(*iter))
      return;
  }
}

bool GetXWindowStack(Window window, std::vector<XID>* windows) {
  std::vector<uint32_t> value32;
  if (!GetArrayProperty(window, "_NET_CLIENT_LIST_STACKING", &value32))
    return false;
  // It's more common to iterate from lowest window to highest,
  // so reverse the vector.
  *windows = std::vector<XID>(value32.rbegin(), value32.rend());
  return true;
}

WindowManagerName GuessWindowManager() {
  std::string name;
  if (!GetWindowManagerName(&name))
    return WM_UNNAMED;
  // These names are taken from the WMs' source code.
  if (name == "awesome")
    return WM_AWESOME;
  if (name == "Blackbox")
    return WM_BLACKBOX;
  if (name == "Compiz" || name == "compiz")
    return WM_COMPIZ;
  if (name == "e16" || name == "Enlightenment")
    return WM_ENLIGHTENMENT;
  if (name == "Fluxbox")
    return WM_FLUXBOX;
  if (name == "i3")
    return WM_I3;
  if (base::StartsWith(name, "IceWM", base::CompareCase::SENSITIVE))
    return WM_ICE_WM;
  if (name == "ion3")
    return WM_ION3;
  if (name == "KWin")
    return WM_KWIN;
  if (name == "matchbox")
    return WM_MATCHBOX;
  if (name == "Metacity")
    return WM_METACITY;
  if (name == "Mutter (Muffin)")
    return WM_MUFFIN;
  if (name == "GNOME Shell")
    return WM_MUTTER;  // GNOME Shell uses Mutter
  if (name == "Mutter")
    return WM_MUTTER;
  if (name == "notion")
    return WM_NOTION;
  if (name == "Openbox")
    return WM_OPENBOX;
  if (name == "qtile")
    return WM_QTILE;
  if (name == "ratpoison")
    return WM_RATPOISON;
  if (name == "stumpwm")
    return WM_STUMPWM;
  if (name == "wmii")
    return WM_WMII;
  if (name == "Xfwm4")
    return WM_XFWM4;
  if (name == "xmonad")
    return WM_XMONAD;
  return WM_OTHER;
}

std::string GuessWindowManagerName() {
  std::string name;
  if (GetWindowManagerName(&name))
    return name;
  return "Unknown";
}

bool IsCompositingManagerPresent() {
  static bool is_compositing_manager_present =
      XGetSelectionOwner(gfx::GetXDisplay(), gfx::GetAtom("_NET_WM_CM_S0")) !=
      x11::None;
  return is_compositing_manager_present;
}

void SetDefaultX11ErrorHandlers() {
  SetX11ErrorHandlers(nullptr, nullptr);
}

bool IsX11WindowFullScreen(XID window) {
  // If _NET_WM_STATE_FULLSCREEN is in _NET_SUPPORTED, use the presence or
  // absence of _NET_WM_STATE_FULLSCREEN in _NET_WM_STATE to determine
  // whether we're fullscreen.
  XAtom fullscreen_atom = gfx::GetAtom("_NET_WM_STATE_FULLSCREEN");
  if (WmSupportsHint(fullscreen_atom)) {
    std::vector<XAtom> atom_properties;
    if (GetAtomArrayProperty(window, "_NET_WM_STATE", &atom_properties)) {
      return base::Contains(atom_properties, fullscreen_atom);
    }
  }

  gfx::Rect window_rect;
  if (!ui::GetOuterWindowBounds(window, &window_rect))
    return false;

  // We can't use display::Screen here because we don't have an aura::Window. So
  // instead just look at the size of the default display.
  //
  // TODO(erg): Actually doing this correctly would require pulling out xrandr,
  // which we don't even do in the desktop screen yet.
  ::XDisplay* display = gfx::GetXDisplay();
  ::Screen* screen = DefaultScreenOfDisplay(display);
  int width = WidthOfScreen(screen);
  int height = HeightOfScreen(screen);
  return window_rect.size() == gfx::Size(width, height);
}

bool WmSupportsHint(XAtom atom) {
  if (!SupportsEWMH())
    return false;

  std::vector<XAtom> supported_atoms;
  if (!GetAtomArrayProperty(GetX11RootWindow(), "_NET_SUPPORTED",
                            &supported_atoms)) {
    return false;
  }

  return base::Contains(supported_atoms, atom);
}

gfx::ICCProfile GetICCProfileForMonitor(int monitor) {
  gfx::ICCProfile icc_profile;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless))
    return icc_profile;
  std::string atom_name;
  if (monitor == 0) {
    atom_name = "_ICC_PROFILE";
  } else {
    atom_name = base::StringPrintf("_ICC_PROFILE_%d", monitor);
  }
  Atom property = gfx::GetAtom(atom_name.c_str());
  if (property != x11::None) {
    Atom prop_type = x11::None;
    int prop_format = 0;
    unsigned long nitems = 0;
    unsigned long nbytes = 0;
    char* property_data = nullptr;
    int result = XGetWindowProperty(
        gfx::GetXDisplay(), DefaultRootWindow(gfx::GetXDisplay()), property, 0,
        kLongLength, x11::False, AnyPropertyType, &prop_type, &prop_format,
        &nitems, &nbytes, reinterpret_cast<unsigned char**>(&property_data));
    if (result == x11::Success) {
      icc_profile = gfx::ICCProfile::FromData(property_data, nitems);
      XFree(property_data);
    }
  }
  return icc_profile;
}

bool IsSyncExtensionAvailable() {
// Chrome for ChromeOS can be run with X11 on a Linux desktop. In this case,
// NotifySwapAfterResize is never called as the compositor does not notify about
// swaps after resize. Thus, simply disable usage of XSyncCounter on ChromeOS
// builds.
//
// TODO(https://crbug.com/1036285): Also, disable sync extension for all ozone
// builds as long as our EGL impl for Ozone/X11 is not mature enough and we do
// not receive swap completions on time, which results in weird resize behaviour
// as X Server waits for the XSyncCounter changes.
#if defined(OS_CHROMEOS) || defined(USE_OZONE)
  return false;
#else
  auto* display = gfx::GetXDisplay();
  int unused;
  static bool result = XSyncQueryExtension(display, &unused, &unused) &&
                       XSyncInitialize(display, &unused, &unused);
  return result;
#endif
}

SkColorType ColorTypeForVisual(void* visual) {
  struct {
    SkColorType color_type;
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
  } color_infos[] = {
      {kRGB_565_SkColorType, 0xf800, 0x7e0, 0x1f},
      {kARGB_4444_SkColorType, 0xf000, 0xf00, 0xf0},
      {kRGBA_8888_SkColorType, 0xff, 0xff00, 0xff0000},
      {kBGRA_8888_SkColorType, 0xff0000, 0xff00, 0xff},
      {kRGBA_1010102_SkColorType, 0x3ff, 0xffc00, 0x3ff00000},
      {kBGRA_1010102_SkColorType, 0x3ff00000, 0xffc00, 0x3ff},
  };
  Visual* vis = reinterpret_cast<Visual*>(visual);
  // When running under Xvfb, a visual may not be set.
  if (!vis->red_mask && !vis->green_mask && !vis->blue_mask)
    return kUnknown_SkColorType;
  for (const auto& color_info : color_infos) {
    if (vis->red_mask == color_info.red_mask &&
        vis->green_mask == color_info.green_mask &&
        vis->blue_mask == color_info.blue_mask) {
      return color_info.color_type;
    }
  }
  LOG(ERROR) << "Unsupported visual with rgb mask 0x" << std::hex
             << vis->red_mask << ", 0x" << vis->green_mask << ", 0x"
             << vis->blue_mask
             << ".  Please report this to https://crbug.com/1025266";
  return kUnknown_SkColorType;
}

XRefcountedMemory::XRefcountedMemory(unsigned char* x11_data, size_t length)
    : x11_data_(length ? x11_data : nullptr), length_(length) {}

const unsigned char* XRefcountedMemory::front() const {
  return x11_data_.get();
}

size_t XRefcountedMemory::size() const {
  return length_;
}

XRefcountedMemory::~XRefcountedMemory() = default;

XScopedCursor::XScopedCursor(::Cursor cursor, XDisplay* display)
    : cursor_(cursor), display_(display) {}

XScopedCursor::~XScopedCursor() {
  reset(0U);
}

::Cursor XScopedCursor::get() const {
  return cursor_;
}

void XScopedCursor::reset(::Cursor cursor) {
  if (cursor_)
    XFreeCursor(display_, cursor_);
  cursor_ = cursor;
}

void XImageDeleter::operator()(XImage* image) const {
  XDestroyImage(image);
}

namespace test {

const XcursorImage* GetCachedXcursorImage(::Cursor cursor) {
  return XCustomCursorCache::GetInstance()->GetXcursorImage(cursor);
}
}  // namespace test

// ----------------------------------------------------------------------------
// These functions are declared in x11_util_internal.h because they require
// XLib.h to be included, and it conflicts with many other headers.
XRenderPictFormat* GetRenderARGB32Format(XDisplay* dpy) {
  static XRenderPictFormat* pictformat = nullptr;
  if (pictformat)
    return pictformat;

  // First look for a 32-bit format which ignores the alpha value
  XRenderPictFormat templ;
  templ.depth = 32;
  templ.type = PictTypeDirect;
  templ.direct.red = 16;
  templ.direct.green = 8;
  templ.direct.blue = 0;
  templ.direct.redMask = 0xff;
  templ.direct.greenMask = 0xff;
  templ.direct.blueMask = 0xff;
  templ.direct.alphaMask = 0;

  static const unsigned long kMask =
      PictFormatType | PictFormatDepth | PictFormatRed | PictFormatRedMask |
      PictFormatGreen | PictFormatGreenMask | PictFormatBlue |
      PictFormatBlueMask | PictFormatAlphaMask;

  pictformat = XRenderFindFormat(dpy, kMask, &templ, 0 /* first result */);

  if (!pictformat) {
    // Not all X servers support xRGB32 formats. However, the XRENDER spec says
    // that they must support an ARGB32 format, so we can always return that.
    pictformat = XRenderFindStandardFormat(dpy, PictStandardARGB32);
    CHECK(pictformat) << "XRENDER ARGB32 not supported.";
  }

  return pictformat;
}

void SetX11ErrorHandlers(XErrorHandler error_handler,
                         XIOErrorHandler io_error_handler) {
  XSetErrorHandler(error_handler ? error_handler : DefaultX11ErrorHandler);
  XSetIOErrorHandler(io_error_handler ? io_error_handler
                                      : DefaultX11IOErrorHandler);
}

// static
XVisualManager* XVisualManager::GetInstance() {
  return base::Singleton<XVisualManager>::get();
}

XVisualManager::XVisualManager()
    : display_(gfx::GetXDisplay()),
      default_visual_id_(0),
      system_visual_id_(0),
      transparent_visual_id_(0),
      using_software_rendering_(false),
      have_gpu_argb_visual_(false) {
  base::AutoLock lock(lock_);
  int visuals_len = 0;
  XVisualInfo visual_template;
  visual_template.screen = DefaultScreen(display_);
  gfx::XScopedPtr<XVisualInfo[]> visual_list(XGetVisualInfo(
      display_, VisualScreenMask, &visual_template, &visuals_len));
  for (int i = 0; i < visuals_len; ++i)
    visuals_[visual_list[i].visualid] =
        std::make_unique<XVisualData>(visual_list[i]);

  XAtom NET_WM_CM_S0 = gfx::GetAtom("_NET_WM_CM_S0");
  using_compositing_wm_ =
      XGetSelectionOwner(display_, NET_WM_CM_S0) != x11::None;

  // Choose the opaque visual.
  default_visual_id_ =
      XVisualIDFromVisual(DefaultVisual(display_, DefaultScreen(display_)));
  system_visual_id_ = default_visual_id_;
  DCHECK(system_visual_id_);
  DCHECK(visuals_.find(system_visual_id_) != visuals_.end());

  // Choose the transparent visual.
  for (const auto& pair : visuals_) {
    // Why support only 8888 ARGB? Because it's all that GTK+ supports. In
    // gdkvisual-x11.cc, they look for this specific visual and use it for
    // all their alpha channel using needs.
    const XVisualInfo& info = pair.second->visual_info;
    if (info.depth == 32 && info.visual->red_mask == 0xff0000 &&
        info.visual->green_mask == 0x00ff00 &&
        info.visual->blue_mask == 0x0000ff) {
      transparent_visual_id_ = info.visualid;
      break;
    }
  }
  if (transparent_visual_id_)
    DCHECK(visuals_.find(transparent_visual_id_) != visuals_.end());
}

XVisualManager::~XVisualManager() = default;

void XVisualManager::ChooseVisualForWindow(bool want_argb_visual,
                                           Visual** visual,
                                           int* depth,
                                           Colormap* colormap,
                                           bool* visual_has_alpha) {
  base::AutoLock lock(lock_);
  bool use_argb = want_argb_visual && using_compositing_wm_ &&
                  (using_software_rendering_ || have_gpu_argb_visual_);
  VisualID visual_id = use_argb && transparent_visual_id_
                           ? transparent_visual_id_
                           : system_visual_id_;

  bool success =
      GetVisualInfoImpl(visual_id, visual, depth, colormap, visual_has_alpha);
  DCHECK(success);
}

bool XVisualManager::GetVisualInfo(VisualID visual_id,
                                   Visual** visual,
                                   int* depth,
                                   Colormap* colormap,
                                   bool* visual_has_alpha) {
  base::AutoLock lock(lock_);
  return GetVisualInfoImpl(visual_id, visual, depth, colormap,
                           visual_has_alpha);
}

bool XVisualManager::OnGPUInfoChanged(bool software_rendering,
                                      VisualID system_visual_id,
                                      VisualID transparent_visual_id) {
  base::AutoLock lock(lock_);
  // TODO(thomasanderson): Cache these visual IDs as a property of the root
  // window so that newly created browser processes can get them immediately.
  if ((system_visual_id && !visuals_.count(system_visual_id)) ||
      (transparent_visual_id && !visuals_.count(transparent_visual_id)))
    return false;
  using_software_rendering_ = software_rendering;
  have_gpu_argb_visual_ = have_gpu_argb_visual_ || transparent_visual_id;
  if (system_visual_id)
    system_visual_id_ = system_visual_id;
  if (transparent_visual_id)
    transparent_visual_id_ = transparent_visual_id;
  return true;
}

bool XVisualManager::ArgbVisualAvailable() const {
  base::AutoLock lock(lock_);
  return using_compositing_wm_ &&
         (using_software_rendering_ || have_gpu_argb_visual_);
}

bool XVisualManager::GetVisualInfoImpl(VisualID visual_id,
                                       Visual** visual,
                                       int* depth,
                                       Colormap* colormap,
                                       bool* visual_has_alpha) {
  auto it = visuals_.find(visual_id);
  if (it == visuals_.end())
    return false;
  XVisualData& visual_data = *it->second;
  const XVisualInfo& visual_info = visual_data.visual_info;

  bool is_default_visual = visual_id == default_visual_id_;

  if (visual)
    *visual = visual_info.visual;
  if (depth)
    *depth = visual_info.depth;
  if (colormap)
    *colormap =
        is_default_visual ? 0 /* CopyFromParent */ : visual_data.GetColormap();
  if (visual_has_alpha) {
    auto popcount = [](auto x) {
      return std::bitset<8 * sizeof(decltype(x))>(x).count();
    };
    *visual_has_alpha = popcount(visual_info.red_mask) +
                            popcount(visual_info.green_mask) +
                            popcount(visual_info.blue_mask) <
                        static_cast<std::size_t>(visual_info.depth);
  }
  return true;
}

XVisualManager::XVisualData::XVisualData(XVisualInfo visual_info)
    : visual_info(visual_info), colormap_(0 /* CopyFromParent */) {}

// Do not XFreeColormap as this would uninstall the colormap even for
// non-Chromium clients.
XVisualManager::XVisualData::~XVisualData() = default;

Colormap XVisualManager::XVisualData::GetColormap() {
  XDisplay* display = gfx::GetXDisplay();
  if (colormap_ == 0 /* CopyFromParent */) {
    colormap_ = XCreateColormap(display, DefaultRootWindow(display),
                                visual_info.visual, AllocNone);
  }
  return colormap_;
}

// ----------------------------------------------------------------------------
// End of x11_util_internal.h

}  // namespace ui
