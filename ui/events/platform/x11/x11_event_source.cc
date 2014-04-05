// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_event_source.h"

#include <glib.h>
#include <X11/extensions/XInput2.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

namespace {

struct GLibX11Source : public GSource {
  // Note: The GLibX11Source is created and destroyed by GLib. So its
  // constructor/destructor may or may not get called.
  XDisplay* display;
  GPollFD* poll_fd;
};

gboolean XSourcePrepare(GSource* source, gint* timeout_ms) {
  GLibX11Source* gxsource = static_cast<GLibX11Source*>(source);
  if (XPending(gxsource->display))
    *timeout_ms = 0;
  else
    *timeout_ms = -1;
  return FALSE;
}

gboolean XSourceCheck(GSource* source) {
  GLibX11Source* gxsource = static_cast<GLibX11Source*>(source);
  return XPending(gxsource->display);
}

gboolean XSourceDispatch(GSource* source,
                         GSourceFunc unused_func,
                         gpointer data) {
  X11EventSource* x11_source = static_cast<X11EventSource*>(data);
  x11_source->DispatchXEvents();
  return TRUE;
}

GSourceFuncs XSourceFuncs = {
  XSourcePrepare,
  XSourceCheck,
  XSourceDispatch,
  NULL
};

int g_xinput_opcode = -1;

bool InitializeXInput2(XDisplay* display) {
  if (!display)
    return false;

  int event, err;

  int xiopcode;
  if (!XQueryExtension(display, "XInputExtension", &xiopcode, &event, &err)) {
    DVLOG(1) << "X Input extension not available.";
    return false;
  }
  g_xinput_opcode = xiopcode;

#if defined(USE_XI2_MT)
  // USE_XI2_MT also defines the required XI2 minor minimum version.
  int major = 2, minor = USE_XI2_MT;
#else
  int major = 2, minor = 0;
#endif
  if (XIQueryVersion(display, &major, &minor) == BadRequest) {
    DVLOG(1) << "XInput2 not supported in the server.";
    return false;
  }
#if defined(USE_XI2_MT)
  if (major < 2 || (major == 2 && minor < USE_XI2_MT)) {
    DVLOG(1) << "XI version on server is " << major << "." << minor << ". "
            << "But 2." << USE_XI2_MT << " is required.";
    return false;
  }
#endif

  return true;
}

bool InitializeXkb(XDisplay* display) {
  if (!display)
    return false;

  int opcode, event, error;
  int major = XkbMajorVersion;
  int minor = XkbMinorVersion;
  if (!XkbQueryExtension(display, &opcode, &event, &error, &major, &minor)) {
    DVLOG(1) << "Xkb extension not available.";
    return false;
  }

  // Ask the server not to send KeyRelease event when the user holds down a key.
  // crbug.com/138092
  Bool supported_return;
  if (!XkbSetDetectableAutoRepeat(display, True, &supported_return)) {
    DVLOG(1) << "XKB not supported in the server.";
    return false;
  }

  return true;
}

}  // namespace

X11EventSource::X11EventSource(XDisplay* display)
    : display_(display),
      x_source_(NULL) {
  CHECK(display_);
  InitializeXInput2(display_);
  InitializeXkb(display_);

  InitXSource();
}

X11EventSource::~X11EventSource() {
  g_source_destroy(x_source_);
  g_source_unref(x_source_);
}

// static
X11EventSource* X11EventSource::GetInstance() {
  return static_cast<X11EventSource*>(PlatformEventSource::GetInstance());
}

////////////////////////////////////////////////////////////////////////////////
// X11EventSource, public

void X11EventSource::DispatchXEvents() {
  DCHECK(display_);
  // Handle all pending events.
  // It may be useful to eventually align this event dispatch with vsync, but
  // not yet.
  while (XPending(display_)) {
    XEvent xevent;
    XNextEvent(display_, &xevent);
    uint32_t action = DispatchEvent(&xevent);
    if (action & POST_DISPATCH_QUIT_LOOP)
      break;
  }
}

void X11EventSource::BlockUntilWindowMapped(XID window) {
  XEvent event;
  do {
    // Block until there's a message of |event_mask| type on |w|. Then remove
    // it from the queue and stuff it in |event|.
    XWindowEvent(display_, window, StructureNotifyMask, &event);
    DispatchEvent(&event);
  } while (event.type != MapNotify);
}

////////////////////////////////////////////////////////////////////////////////
// X11EventSource, private

void X11EventSource::InitXSource() {
  CHECK(!x_source_);
  CHECK(display_) << "Unable to get connection to X server";

  x_poll_.reset(new GPollFD());
  x_poll_->fd = ConnectionNumber(display_);
  x_poll_->events = G_IO_IN;
  x_poll_->revents = 0;

  GLibX11Source* glib_x_source = static_cast<GLibX11Source*>
      (g_source_new(&XSourceFuncs, sizeof(GLibX11Source)));
  glib_x_source->display = display_;
  glib_x_source->poll_fd = x_poll_.get();

  x_source_ = glib_x_source;
  g_source_add_poll(x_source_, x_poll_.get());
  g_source_set_can_recurse(x_source_, TRUE);
  g_source_set_callback(x_source_, NULL, this, NULL);
  g_source_attach(x_source_, g_main_context_default());
}

uint32_t X11EventSource::DispatchEvent(XEvent* xevent) {
  bool have_cookie = false;
  if (xevent->type == GenericEvent &&
      XGetEventData(xevent->xgeneric.display, &xevent->xcookie)) {
    have_cookie = true;
  }

  // TODO(sad): Remove this once all MessagePumpObservers are turned into
  // PlatformEventObservers.
  base::MessagePumpX11::Current()->WillProcessXEvent(xevent);
  uint32_t action = PlatformEventSource::DispatchEvent(xevent);
  base::MessagePumpX11::Current()->DidProcessXEvent(xevent);

  if (have_cookie)
    XFreeEventData(xevent->xgeneric.display, &xevent->xcookie);
  return action;
}

scoped_ptr<PlatformEventSource> PlatformEventSource::CreateDefault() {
  return scoped_ptr<PlatformEventSource>(
      new X11EventSource(gfx::GetXDisplay()));
}

}  // namespace ui
