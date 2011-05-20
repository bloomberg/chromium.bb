// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/touchui/touch_factory.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XIproto.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/base/x/x11_util.h"

namespace {

// The X cursor is hidden if it is idle for kCursorIdleSeconds seconds.
int kCursorIdleSeconds = 5;

// Given the TouchParam, return the correspoding valuator index using
// the X device information through Atom name matching.
char FindTPValuator(Display* display,
                    XIDeviceInfo* info,
                    views::TouchFactory::TouchParam touch_param) {
  // Lookup table for mapping TouchParam to Atom string used in X.
  // A full set of Atom strings can be found at xserver-properties.h.
  // For Slot ID, See this chromeos revision: http://git.chromium.org/gitweb/?
  //        p=chromiumos/overlays/chromiumos-overlay.git;
  //        a=commit;h=9164d0a75e48c4867e4ef4ab51f743ae231c059a
  static struct {
    views::TouchFactory::TouchParam tp;
    const char* atom;
  } kTouchParamAtom[] = {
    { views::TouchFactory::TP_TOUCH_MAJOR, "Abs MT Touch Major" },
    { views::TouchFactory::TP_TOUCH_MINOR, "Abs MT Touch Minor" },
    { views::TouchFactory::TP_ORIENTATION, "Abs MT Orientation" },
    { views::TouchFactory::TP_SLOT_ID,     "Abs MT Slot ID" },
    { views::TouchFactory::TP_TRACKING_ID, "Abs MT Tracking ID" },
    { views::TouchFactory::TP_LAST_ENTRY, NULL },
  };

  const char* atom_tp = NULL;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTouchParamAtom); i++) {
    if (touch_param == kTouchParamAtom[i].tp) {
      atom_tp = kTouchParamAtom[i].atom;
      break;
    }
  }

  if (!atom_tp)
    return -1;

  for (int i = 0; i < info->num_classes; i++) {
    if (info->classes[i]->type != XIValuatorClass)
      continue;
    XIValuatorClassInfo* v =
        reinterpret_cast<XIValuatorClassInfo*>(info->classes[i]);

    const char* atom = XGetAtomName(display, v->label);
    if (atom && strcmp(atom, atom_tp) == 0)
      return v->number;
  }

  return -1;
}

// Setup XInput2 select for the GtkWidget.
gboolean GtkWidgetRealizeCallback(GSignalInvocationHint* hint, guint nparams,
                                  const GValue* pvalues, gpointer data) {
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(pvalues));
  GdkWindow* window = widget->window;
  views::TouchFactory* factory = static_cast<views::TouchFactory*>(data);

  if (GDK_WINDOW_TYPE(window) != GDK_WINDOW_TOPLEVEL &&
      GDK_WINDOW_TYPE(window) != GDK_WINDOW_CHILD &&
      GDK_WINDOW_TYPE(window) != GDK_WINDOW_DIALOG)
    return true;

  factory->SetupXI2ForXWindow(GDK_WINDOW_XID(window));
  return true;
}

// We need to capture all the GDK windows that get created, and start
// listening for XInput2 events. So we setup a callback to the 'realize'
// signal for GTK+ widgets, so that whenever the signal triggers for any
// GtkWidget, which means the GtkWidget should now have a GdkWindow, we can
// setup XInput2 events for the GdkWindow.
guint realize_signal_id = 0;
guint realize_hook_id = 0;

void SetupGtkWidgetRealizeNotifier(views::TouchFactory* factory) {
  gpointer klass = g_type_class_ref(GTK_TYPE_WIDGET);

  g_signal_parse_name("realize", GTK_TYPE_WIDGET,
                      &realize_signal_id, NULL, FALSE);
  realize_hook_id = g_signal_add_emission_hook(realize_signal_id, 0,
      GtkWidgetRealizeCallback, static_cast<gpointer>(factory), NULL);

  g_type_class_unref(klass);
}

void RemoveGtkWidgetRealizeNotifier() {
  if (realize_signal_id != 0)
    g_signal_remove_emission_hook(realize_signal_id, realize_hook_id);
  realize_signal_id = 0;
  realize_hook_id = 0;
}

}  // namespace

namespace views {

// static
TouchFactory* TouchFactory::GetInstance() {
  return Singleton<TouchFactory>::get();
}

TouchFactory::TouchFactory()
    : is_cursor_visible_(true),
      keep_mouse_cursor_(false),
      cursor_timer_(),
      pointer_device_lookup_(),
      touch_device_list_(),
      slots_used_() {
  char nodata[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  XColor black;
  black.red = black.green = black.blue = 0;
  Display* display = ui::GetXDisplay();
  Pixmap blank = XCreateBitmapFromData(display, ui::GetX11RootWindow(),
                                       nodata, 8, 8);
  invisible_cursor_ = XCreatePixmapCursor(display, blank, blank,
                                          &black, &black, 0, 0);
  arrow_cursor_ = XCreateFontCursor(display, XC_arrow);

  SetCursorVisible(false, false);
  UpdateDeviceList(display);

  // TODO(sad): Here, we only setup so that the X windows created by GTK+ are
  // setup for XInput2 events. We need a way to listen for XInput2 events for X
  // windows created by other means (e.g. for context menus).
  SetupGtkWidgetRealizeNotifier(this);

  // Make sure the list of devices is kept up-to-date by listening for
  // XI_HierarchyChanged event on the root window.
  unsigned char mask[XIMaskLen(XI_LASTEVENT)];
  memset(mask, 0, sizeof(mask));

  XISetMask(mask, XI_HierarchyChanged);

  XIEventMask evmask;
  evmask.deviceid = XIAllDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  XISelectEvents(display, ui::GetX11RootWindow(), &evmask, 1);
}

TouchFactory::~TouchFactory() {
  SetCursorVisible(true, false);
  Display* display = ui::GetXDisplay();
  XFreeCursor(display, invisible_cursor_);
  XFreeCursor(display, arrow_cursor_);

  RemoveGtkWidgetRealizeNotifier();
}

void TouchFactory::UpdateDeviceList(Display* display) {
  // Detect touch devices.
  // NOTE: The new API for retrieving the list of devices (XIQueryDevice) does
  // not provide enough information to detect a touch device. As a result, the
  // old version of query function (XListInputDevices) is used instead.
  // If XInput2 is not supported, this will return null (with count of -1) so
  // we assume there cannot be any touch devices.
  int count = 0;
  touch_device_lookup_.reset();
  touch_device_list_.clear();
  XDeviceInfo* devlist = XListInputDevices(display, &count);
  for (int i = 0; i < count; i++) {
    if (devlist[i].type) {
      const char* devtype = XGetAtomName(display, devlist[i].type);
      if (devtype && !strcmp(devtype, XI_TOUCHSCREEN)) {
        touch_device_lookup_[devlist[i].id] = true;
        touch_device_list_.push_back(devlist[i].id);
      }
    }
  }
  if (devlist)
    XFreeDeviceList(devlist);

  // Instead of asking X for the list of devices all the time, let's maintain a
  // list of pointer devices we care about.
  // It should not be necessary to select for slave devices. XInput2 provides
  // enough information to the event callback to decide which slave device
  // triggered the event, thus decide whether the 'pointer event' is a
  // 'mouse event' or a 'touch event'.
  // However, on some desktops, some events from a master pointer are
  // not delivered to the client. So we select for slave devices instead.
  // If the touch device has 'GrabDevice' set and 'SendCoreEvents' unset (which
  // is possible), then the device is detected as a floating device, and a
  // floating device is not connected to a master device. So it is necessary to
  // also select on the floating devices.
  pointer_device_lookup_.reset();
  XIDeviceInfo* devices = XIQueryDevice(display, XIAllDevices, &count);
  for (int i = 0; i < count; i++) {
    XIDeviceInfo* devinfo = devices + i;
    if (devinfo->use == XIFloatingSlave || devinfo->use == XISlavePointer) {
      pointer_device_lookup_[devinfo->deviceid] = true;
    }
  }
  if (devices)
    XIFreeDeviceInfo(devices);

  SetupValuator();
}

bool TouchFactory::ShouldProcessXI2Event(XEvent* xev) {
  DCHECK_EQ(GenericEvent, xev->type);

  XGenericEventCookie* cookie = &xev->xcookie;
  if (cookie->evtype != XI_ButtonPress &&
      cookie->evtype != XI_ButtonRelease &&
      cookie->evtype != XI_Motion)
    return true;

  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(cookie->data);
  return pointer_device_lookup_[xiev->sourceid];
}

void TouchFactory::SetupXI2ForXWindow(Window window) {
  // Setup mask for mouse events. It is possible that a device is loaded/plugged
  // in after we have setup XInput2 on a window. In such cases, we need to
  // either resetup XInput2 for the window, so that we get events from the new
  // device, or we need to listen to events from all devices, and then filter
  // the events from uninteresting devices. We do the latter because that's
  // simpler.

  Display* display = ui::GetXDisplay();

  unsigned char mask[XIMaskLen(XI_LASTEVENT)];
  memset(mask, 0, sizeof(mask));

  XISetMask(mask, XI_ButtonPress);
  XISetMask(mask, XI_ButtonRelease);
  XISetMask(mask, XI_Motion);

  XIEventMask evmask;
  evmask.deviceid = XIAllDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  XISelectEvents(display, window, &evmask, 1);
  XFlush(display);
}

void TouchFactory::SetTouchDeviceList(
    const std::vector<unsigned int>& devices) {
  touch_device_lookup_.reset();
  touch_device_list_.clear();
  for (std::vector<unsigned int>::const_iterator iter = devices.begin();
       iter != devices.end(); ++iter) {
    DCHECK(*iter < touch_device_lookup_.size());
    touch_device_lookup_[*iter] = true;
    touch_device_list_.push_back(*iter);
  }

  SetupValuator();
}

bool TouchFactory::IsTouchDevice(unsigned deviceid) const {
  return deviceid < touch_device_lookup_.size() ?
      touch_device_lookup_[deviceid] : false;
}

bool TouchFactory::IsSlotUsed(int slot) const {
  CHECK_LT(slot, kMaxTouchPoints);
  return slots_used_[slot];
}

void TouchFactory::SetSlotUsed(int slot, bool used) {
  CHECK_LT(slot, kMaxTouchPoints);
  slots_used_[slot] = used;
}

bool TouchFactory::GrabTouchDevices(Display* display, ::Window window) {
  if (touch_device_list_.empty())
    return true;

  unsigned char mask[XIMaskLen(XI_LASTEVENT)];
  bool success = true;

  memset(mask, 0, sizeof(mask));
  XISetMask(mask, XI_ButtonPress);
  XISetMask(mask, XI_ButtonRelease);
  XISetMask(mask, XI_Motion);

  XIEventMask evmask;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  for (std::vector<int>::const_iterator iter =
       touch_device_list_.begin();
       iter != touch_device_list_.end(); ++iter) {
    evmask.deviceid = *iter;
    Status status = XIGrabDevice(display, *iter, window, CurrentTime, None,
                                 GrabModeAsync, GrabModeAsync, False, &evmask);
    success = success && status == GrabSuccess;
  }

  return success;
}

bool TouchFactory::UngrabTouchDevices(Display* display) {
  bool success = true;
  for (std::vector<int>::const_iterator iter =
       touch_device_list_.begin();
       iter != touch_device_list_.end(); ++iter) {
    Status status = XIUngrabDevice(display, *iter, CurrentTime);
    success = success && status == GrabSuccess;
  }
  return success;
}

void TouchFactory::SetCursorVisible(bool show, bool start_timer) {
  // The cursor is going to be shown. Reset the timer for hiding it.
  if (show && start_timer) {
    cursor_timer_.Stop();
    cursor_timer_.Start(base::TimeDelta::FromSeconds(kCursorIdleSeconds),
        this, &TouchFactory::HideCursorForInactivity);
  } else {
    cursor_timer_.Stop();
  }

  if (show == is_cursor_visible_)
    return;

  is_cursor_visible_ = show;

  Display* display = ui::GetXDisplay();
  Window window = DefaultRootWindow(display);

  if (is_cursor_visible_) {
    XDefineCursor(display, window, arrow_cursor_);
  } else {
    XDefineCursor(display, window, invisible_cursor_);
  }
}

void TouchFactory::SetupValuator() {
  memset(valuator_lookup_, -1, sizeof(valuator_lookup_));

  Display* display = ui::GetXDisplay();
  int ndevice;
  XIDeviceInfo* info_list = XIQueryDevice(display, XIAllDevices, &ndevice);

  for (int i = 0; i < ndevice; i++) {
    XIDeviceInfo* info = info_list + i;

    if (!IsTouchDevice(info->deviceid))
      continue;

    for (int i = 0; i < TP_LAST_ENTRY; i++) {
      TouchParam tp = static_cast<TouchParam>(i);
      valuator_lookup_[info->deviceid][i] = FindTPValuator(display, info, tp);
    }
  }

  if (info_list)
    XIFreeDeviceInfo(info_list);
}

bool TouchFactory::ExtractTouchParam(const XEvent& xev,
                                     TouchParam tp,
                                     float* value) {
  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  if (xiev->sourceid >= kMaxDeviceNum)
    return false;
  int v = valuator_lookup_[xiev->sourceid][tp];
  if (v >= 0 && XIMaskIsSet(xiev->valuators.mask, v)) {
    *value = xiev->valuators.values[v];
    return true;
  }

  return false;
}

}  // namespace views
