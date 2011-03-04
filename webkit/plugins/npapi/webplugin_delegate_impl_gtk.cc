// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/webplugin_delegate_impl.h"

#include <string>
#include <vector>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/metrics/stats_counters.h"
#include "base/string_util.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/gfx/blit.h"
#include "webkit/plugins/npapi/gtk_plugin_container.h"
#include "webkit/plugins/npapi/plugin_constants_win.h"
#include "webkit/plugins/npapi/plugin_instance.h"
#include "webkit/plugins/npapi/plugin_lib.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/plugin_stream_url.h"
#include "webkit/plugins/npapi/webplugin.h"

#include "third_party/npapi/bindings/npapi_x11.h"

using WebKit::WebCursorInfo;
using WebKit::WebKeyboardEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;

namespace webkit {
namespace npapi {

WebPluginDelegateImpl::WebPluginDelegateImpl(
    gfx::PluginWindowHandle containing_view,
    PluginInstance *instance)
    : windowed_handle_(0),
      windowed_did_set_window_(false),
      windowless_(false),
      plugin_(NULL),
      instance_(instance),
      windowless_shm_pixmap_(None),
      pixmap_(NULL),
      first_event_time_(-1.0),
      plug_(NULL),
      socket_(NULL),
      parent_(containing_view),
      quirks_(0),
      handle_event_depth_(0),
      first_set_window_call_(true),
      plugin_has_focus_(false),
      has_webkit_focus_(false),
      containing_view_has_focus_(true),
      creation_succeeded_(false) {
  memset(&window_, 0, sizeof(window_));
  if (instance_->mime_type() == "application/x-shockwave-flash") {
    // Flash is tied to Firefox's whacky behavior with windowless plugins. See
    // comments in WindowlessPaint.
    // TODO(viettrungluu): PLUGIN_QUIRK_WINDOWLESS_NO_RIGHT_CLICK: Don't allow
    // right-clicks in windowless content since Flash 10.1 (initial release, at
    // least) hangs in that case. Remove this once Flash is fixed.
    quirks_ |= PLUGIN_QUIRK_WINDOWLESS_OFFSET_WINDOW_TO_DRAW
        | PLUGIN_QUIRK_WINDOWLESS_INVALIDATE_AFTER_SET_WINDOW
        | PLUGIN_QUIRK_WINDOWLESS_NO_RIGHT_CLICK;
  }

  // TODO(evanm): I played with this for quite a while but couldn't
  // figure out a way to make Flash not crash unless I didn't call
  // NPP_SetWindow.
  // However, after piman's grand refactor of windowed plugins, maybe
  // this is no longer necessary.
  quirks_ |= PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY;
}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
  DestroyInstance();

  if (!windowless_)
    WindowedDestroyWindow();

  if (window_.ws_info) {
    // We only ever use ws_info as an NPSetWindowCallbackStruct.
    delete static_cast<NPSetWindowCallbackStruct*>(window_.ws_info);
  }

  if (pixmap_) {
    g_object_unref(pixmap_);
    pixmap_ = NULL;
  }
}

bool WebPluginDelegateImpl::PlatformInitialize() {
  gfx::PluginWindowHandle handle =
      windowless_ ? 0 : gtk_plug_get_id(GTK_PLUG(plug_));
  plugin_->SetWindow(handle);
  return true;
}

void WebPluginDelegateImpl::PlatformDestroyInstance() {
  // Nothing to do here.
}

void WebPluginDelegateImpl::Paint(WebKit::WebCanvas* canvas,
                                  const gfx::Rect& rect) {
  if (!windowless_)
    return;
  cairo_t* context = canvas->beginPlatformPaint();
  WindowlessPaint(context, rect);
  canvas->endPlatformPaint();
}

void WebPluginDelegateImpl::Print(cairo_t* context) {
  NOTIMPLEMENTED();
}

void WebPluginDelegateImpl::InstallMissingPlugin() {
  NOTIMPLEMENTED();
}

bool WebPluginDelegateImpl::WindowedCreatePlugin() {
  DCHECK(!windowed_handle_);
  DCHECK(!plug_);

  // NPP_GetValue() might write 4 bytes of data to this variable.  Don't use a
  // single byte bool, use an int instead and make sure it is initialized.
  int xembed = 0;
  NPError err = instance_->NPP_GetValue(NPPVpluginNeedsXEmbed, &xembed);
  if (err != NPERR_NO_ERROR || !xembed) {
    NOTIMPLEMENTED() << " windowed plugin but without xembed. "
      "See http://code.google.com/p/chromium/issues/detail?id=38229";
    return false;
  }

  // Passing 0 as the socket XID creates a plug without plugging it in a socket
  // yet, so that it can be latter added with gtk_socket_add_id().
  plug_ = gtk_plug_new(0);
  gtk_widget_show(plug_);
  socket_ = gtk_socket_new();
  gtk_widget_show(socket_);
  gtk_container_add(GTK_CONTAINER(plug_), socket_);
  gtk_widget_show_all(plug_);

  // Prevent the plug from being destroyed if the browser kills the container
  // window.
  g_signal_connect(plug_, "delete-event", G_CALLBACK(gtk_true), NULL);
  // Prevent the socket from being destroyed when the plugin removes itself.
  g_signal_connect(socket_, "plug_removed", G_CALLBACK(gtk_true), NULL);

  windowed_handle_ = gtk_socket_get_id(GTK_SOCKET(socket_));

  window_.window = reinterpret_cast<void*>(windowed_handle_);

  if (!window_.ws_info)
    window_.ws_info = new NPSetWindowCallbackStruct;
  NPSetWindowCallbackStruct* extra =
      static_cast<NPSetWindowCallbackStruct*>(window_.ws_info);
  extra->display = GDK_DISPLAY();
  extra->visual = DefaultVisual(GDK_DISPLAY(), 0);
  extra->depth = DefaultDepth(GDK_DISPLAY(), 0);
  extra->colormap = DefaultColormap(GDK_DISPLAY(), 0);

  return true;
}

void WebPluginDelegateImpl::WindowedDestroyWindow() {
  if (plug_) {
    plugin_->WillDestroyWindow(gtk_plug_get_id(GTK_PLUG(plug_)));

    gtk_widget_destroy(plug_);
    plug_ = NULL;
    socket_ = NULL;
    windowed_handle_ = 0;
  }
}

bool WebPluginDelegateImpl::WindowedReposition(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  if (window_rect == window_rect_ && clip_rect == clip_rect_)
    return false;

  window_rect_ = window_rect;
  clip_rect_ = clip_rect;

  return true;
}

void WebPluginDelegateImpl::WindowedSetWindow() {
  if (!instance_)
    return;

  if (!windowed_handle_) {
    NOTREACHED();
    return;
  }

  // See https://bugzilla.mozilla.org/show_bug.cgi?id=108347
  // If we call NPP_SetWindow with a <= 0 width or height, problems arise in
  // Flash (and possibly other plugins).
  // TODO(piman): the Mozilla code suggests that for the Java plugin, we should
  // still call NPP_SetWindow in that case. We need to verify that.
  if (window_rect_.width() <= 0 || window_rect_.height() <= 0) {
    return;
  }

  instance()->set_window_handle(windowed_handle_);

  DCHECK(!instance()->windowless());

  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();

  //window_.window = windowed_handle_;
  window_.type = NPWindowTypeWindow;

  // Reset this flag before entering the instance in case of side-effects.
  windowed_did_set_window_ = true;

  NPError err = instance()->NPP_SetWindow(&window_);
  DCHECK(err == NPERR_NO_ERROR);
}

void WebPluginDelegateImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  // Only resend to the instance if the geometry has changed.
  if (window_rect == window_rect_ && clip_rect == clip_rect_)
    return;

  clip_rect_ = clip_rect;
  window_rect_ = window_rect;
  WindowlessSetWindow();
}

void WebPluginDelegateImpl::EnsurePixmapAtLeastSize(int width, int height) {
  if (pixmap_) {
    gint cur_width, cur_height;
    gdk_drawable_get_size(pixmap_, &cur_width, &cur_height);
    if (cur_width >= width && cur_height >= height)
      return;  // We are already the appropriate size.

    // Otherwise, we need to recreate ourselves.
    g_object_unref(pixmap_);
    pixmap_ = NULL;
  }

  // |sys_visual| is owned by gdk; we shouldn't free it.
  GdkVisual* sys_visual = gdk_visual_get_system();
  pixmap_ = gdk_pixmap_new(NULL,  // use width/height/depth params
                           std::max(1, width), std::max(1, height),
                           sys_visual->depth);
  GdkColormap* colormap = gdk_colormap_new(gdk_visual_get_system(),
                                           FALSE);
  gdk_drawable_set_colormap(GDK_DRAWABLE(pixmap_), colormap);
  // The GdkDrawable now owns the GdkColormap.
  g_object_unref(colormap);
}

#ifdef DEBUG_RECTANGLES
namespace {

// Draw a rectangle on a Cairo context.
// Useful for debugging various rectangles involved in drawing plugins.
void DrawDebugRectangle(cairo_t* cairo,
                        const gfx::Rect& rect,
                        float r, float g, float b) {
  cairo_set_source_rgba(cairo, r, g, b, 0.5);
  cairo_rectangle(cairo, rect.x(), rect.y(),
                  rect.width(), rect.height());
  cairo_stroke(cairo);
}

}  // namespace
#endif

void WebPluginDelegateImpl::WindowlessPaint(cairo_t* context,
                                            const gfx::Rect& damage_rect) {
  // Compare to:
  // http://mxr.mozilla.org/firefox/source/layout/generic/nsObjectFrame.cpp:
  // nsPluginInstanceOwner::Renderer::NativeDraw().

  DCHECK(context);

  // TODO(darin): we should avoid calling NPP_SetWindow here since it may
  // cause page layout to be invalidated.

  // The actual dirty region is just the intersection of the plugin window and
  // the clip window with the damage region. However, the plugin wants to draw
  // relative to the containing window's origin, so our pixmap must be from the
  // window's origin down to the bottom-right edge of the dirty region.
  //
  // Typical case:
  // X-----------------------------------+-----------------------------+
  // |                                   |                             |
  // |    pixmap     +-------------------+                             |
  // |               |   damage          |                window       |
  // |               |                   |                             |
  // |           +---+-------------------+-------------+               |
  // |           |   |                   |   clip      |               |
  // |       +---+---+-------------------+----------+  |               |
  // |       |   |   |                   |          |  |               |
  // |       |   |   | draw              |          |  |               |
  // |       |   |   |                   |          |  |               |
  // +-------+---+---+-------------------+----------+--+               |
  // |       |       |                   |          |                  |
  // |       |       +-------------------+          |                  |
  // |       |                                      |                  |
  // |       |        plugin                        |                  |
  // |       +--------------------------------------+                  |
  // |                                                                 |
  // |                                                                 |
  // +-----------------------------------------------------------------+
  // X = origin
  //
  // NPAPI doesn't properly define which coordinates each of
  // - window.clipRect, window.x and window.y in the SetWindow call
  // - x and y in GraphicsExpose HandleEvent call
  // are relative to, nor does it define what the pixmap is relative to.
  //
  // Any sane values for them just don't work with the flash plugin. Firefox
  // has some interesting behavior. Experiments showed that:
  // - window.clipRect is always in the same space as window.x and window.y
  // - in the first SetWindow call, or when scrolling, window.x and window.y are
  // the coordinates of the plugin relative to the window.
  // - whenever only a part of the plugin is drawn, Firefox issues a SetWindow
  // call before each GraphicsExpose event, that sets the drawing origin to
  // (0, 0) as if the plugin was scrolled to be partially out of the view. The
  // GraphicsExpose event has coordinates relative to the "window" (assuming
  // that virtual scroll). The pixmap is also relative to the window. It always
  // sets the clip rect to the draw rect.
  //
  // Attempts to deviate from that makes Flash render at the wrong place in the
  // pixmap, or render the wrong pixels.
  //
  // Flash plugin:
  // X-----------------------------------------------------------------+
  // |                                                                 |
  // |               +-------------------+        "real" window        |
  // |               |   damage          |                             |
  // |               |                   |                             |
  // |           +---+-------------------+-------------+               |
  // |           |   |                   | "real" clip |               |
  // |       +---+---O===================#==========#==#===============#
  // |       |   |   H draw              |          |  |               H
  // |       |   |   H = pixmap          |          |  |               H
  // |       |   |   H = "apparent" clip |          |  |               H
  // |       +   +---#-------------------+----------+--+               H
  // |       |       H                   |          |                  H
  // |       |       H-------------------+          |                  H
  // |       |       H                              |                  H
  // |       |       H  plugin                      |                  H
  // |       +-------#------------------------------+                  H
  // |               H                                                 H
  // |               H                  "apparent" window              H
  // +---------------#=================================================#
  // X = "real" origin
  // O = "apparent" origin
  // "real" means as seen by Chrome
  // "apparent" means as seen by the plugin.

  gfx::Rect draw_rect = window_rect_.Intersect(damage_rect);

  // clip_rect_ is relative to the plugin
  gfx::Rect clip_rect_window = clip_rect_;
  clip_rect_window.Offset(window_rect_.x(), window_rect_.y());
  draw_rect = draw_rect.Intersect(clip_rect_window);

  // These offsets represent by how much the view is shifted to accomodate
  // Flash (the coordinates of X relative to O in the diagram above).
  int offset_x = 0;
  int offset_y = 0;
  if (quirks_ & PLUGIN_QUIRK_WINDOWLESS_OFFSET_WINDOW_TO_DRAW) {
    offset_x = -draw_rect.x();
    offset_y = -draw_rect.y();
    window_.clipRect.top = 0;
    window_.clipRect.left = 0;
    window_.clipRect.bottom = draw_rect.height();
    window_.clipRect.right = draw_rect.width();
    window_.height = window_rect_.height();
    window_.width = window_rect_.width();
    window_.x = window_rect_.x() - draw_rect.x();
    window_.y = window_rect_.y() - draw_rect.y();
    window_.type = NPWindowTypeDrawable;
    DCHECK(window_.ws_info);
    NPError err = instance()->NPP_SetWindow(&window_);
    DCHECK_EQ(err, NPERR_NO_ERROR);
  }

  gfx::Rect pixmap_draw_rect = draw_rect;
  pixmap_draw_rect.Offset(offset_x, offset_y);

  gfx::Rect pixmap_rect(0, 0,
                        pixmap_draw_rect.right(),
                        pixmap_draw_rect.bottom());

  // Construct the paint message, targeting the pixmap.
  NPEvent np_event = {0};
  XGraphicsExposeEvent &event = np_event.xgraphicsexpose;
  event.type = GraphicsExpose;
  event.x = pixmap_draw_rect.x();
  event.y = pixmap_draw_rect.y();
  event.width = pixmap_draw_rect.width();
  event.height = pixmap_draw_rect.height();
  event.display = GDK_DISPLAY();

  if (windowless_shm_pixmap_ != None) {
    Pixmap pixmap = None;
    GC xgc = NULL;
    Display* display = event.display;
    gfx::Rect plugin_draw_rect = draw_rect;

    // Make plugin_draw_rect relative to the plugin window.
    plugin_draw_rect.Offset(-window_rect_.x(), -window_rect_.y());

    // In case the drawing area does not start with the plugin window origin,
    // we can not let the plugin directly draw over the shared memory pixmap.
    if (plugin_draw_rect.x() != pixmap_draw_rect.x() ||
        plugin_draw_rect.y() != pixmap_draw_rect.y()) {
      pixmap = XCreatePixmap(display, windowless_shm_pixmap_,
                             std::max(1, pixmap_rect.width()),
                             std::max(1, pixmap_rect.height()),
                             DefaultDepth(display, 0));
      xgc = XCreateGC(display, windowless_shm_pixmap_, 0, NULL);
      // Copy the current image into the pixmap, so the plugin can draw over it.
      XCopyArea(display, windowless_shm_pixmap_, pixmap, xgc,
                plugin_draw_rect.x(), plugin_draw_rect.y(),
                pixmap_draw_rect.width(), pixmap_draw_rect.height(),
                pixmap_draw_rect.x(), pixmap_draw_rect.y());

      event.drawable = pixmap;
    } else {
      event.drawable = windowless_shm_pixmap_;
    }

    // Tell the plugin to paint into the pixmap.
    static base::StatsRate plugin_paint("Plugin.Paint");
    base::StatsScope<base::StatsRate> scope(plugin_paint);
    NPError err = instance()->NPP_HandleEvent(&np_event);
    DCHECK_EQ(err, NPERR_NO_ERROR);

    if (pixmap != None) {
      // Copy the rendered image pixmap back into the shm pixmap
      // and thus the drawing buffer.
      XCopyArea(display, pixmap, windowless_shm_pixmap_, xgc,
                pixmap_draw_rect.x(), pixmap_draw_rect.y(),
                pixmap_draw_rect.width(), pixmap_draw_rect.height(),
                plugin_draw_rect.x(), plugin_draw_rect.y());
      XSync(display, FALSE);
      if (xgc)
        XFreeGC(display, xgc);
      XFreePixmap(display, pixmap);
    } else {
      XSync(display, FALSE);
    }
  } else {
    EnsurePixmapAtLeastSize(pixmap_rect.width(), pixmap_rect.height());

    // Copy the current image into the pixmap, so the plugin can draw over
    // this background.
    cairo_t* cairo = gdk_cairo_create(pixmap_);
    BlitContextToContext(cairo, pixmap_draw_rect, context, draw_rect.origin());
    cairo_destroy(cairo);

    event.drawable = GDK_PIXMAP_XID(pixmap_);

    // Tell the plugin to paint into the pixmap.
    static base::StatsRate plugin_paint("Plugin.Paint");
    base::StatsScope<base::StatsRate> scope(plugin_paint);
    NPError err = instance()->NPP_HandleEvent(&np_event);
    DCHECK_EQ(err, NPERR_NO_ERROR);

    cairo_save(context);
    // Now copy the rendered image pixmap back into the drawing buffer.
    gdk_cairo_set_source_pixmap(context, pixmap_, -offset_x, -offset_y);
    cairo_rectangle(context, draw_rect.x(), draw_rect.y(),
                    draw_rect.width(), draw_rect.height());
    cairo_clip(context);
    cairo_paint(context);

#ifdef DEBUG_RECTANGLES
    // Draw some debugging rectangles.
    // Pixmap rect = blue.
    DrawDebugRectangle(context, pixmap_rect, 0, 0, 1);
    // Drawing rect = red.
    DrawDebugRectangle(context, draw_rect, 1, 0, 0);
#endif
    cairo_restore(context);
  }
}

void WebPluginDelegateImpl::WindowlessSetWindow() {
  if (!instance())
    return;

  if (window_rect_.IsEmpty())  // wait for geometry to be set.
    return;

  DCHECK(instance()->windowless());
  // Mozilla docs say that this window param is not used for windowless
  // plugins; rather, the window is passed during the GraphicsExpose event.
  DCHECK(window_.window == 0);

  window_.clipRect.top = clip_rect_.y() + window_rect_.y();
  window_.clipRect.left = clip_rect_.x() + window_rect_.x();
  window_.clipRect.bottom =
      clip_rect_.y() + clip_rect_.height() + window_rect_.y();
  window_.clipRect.right =
      clip_rect_.x() + clip_rect_.width() + window_rect_.x();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();
  window_.type = NPWindowTypeDrawable;

  if (!window_.ws_info)
    window_.ws_info = new NPSetWindowCallbackStruct;
  NPSetWindowCallbackStruct* extra =
      static_cast<NPSetWindowCallbackStruct*>(window_.ws_info);
  extra->display = GDK_DISPLAY();
  extra->visual = DefaultVisual(GDK_DISPLAY(), 0);
  extra->depth = DefaultDepth(GDK_DISPLAY(), 0);
  extra->colormap = DefaultColormap(GDK_DISPLAY(), 0);

  NPError err = instance()->NPP_SetWindow(&window_);
  DCHECK(err == NPERR_NO_ERROR);
  if (quirks_ & PLUGIN_QUIRK_WINDOWLESS_INVALIDATE_AFTER_SET_WINDOW) {
    // After a NPP_SetWindow, Flash cancels its timer that generates the
    // invalidates until it gets a paint event, but doesn't explicitly call
    // NPP_InvalidateRect.
    plugin_->InvalidateRect(clip_rect_);
  }
}

bool WebPluginDelegateImpl::PlatformSetPluginHasFocus(bool focused) {
  DCHECK(instance()->windowless());

  NPEvent np_event = {0};
  XFocusChangeEvent &event = np_event.xfocus;
  event.type = focused ? FocusIn : FocusOut;
  event.display = GDK_DISPLAY();
  // Same values as Firefox. .serial and .window stay 0.
  event.mode = -1;
  event.detail = NotifyDetailNone;
  instance()->NPP_HandleEvent(&np_event);
  return true;
}

// Converts a WebInputEvent::Modifiers bitfield into a
// corresponding X modifier state.
static int GetXModifierState(int modifiers) {
  int x_state = 0;
  if (modifiers & WebInputEvent::ControlKey)
    x_state |= ControlMask;
  if (modifiers & WebInputEvent::ShiftKey)
    x_state |= ShiftMask;
  if (modifiers & WebInputEvent::AltKey)
    x_state |= Mod1Mask;
  if (modifiers & WebInputEvent::MetaKey)
    x_state |= Mod2Mask;
  if (modifiers & WebInputEvent::LeftButtonDown)
    x_state |= Button1Mask;
  if (modifiers & WebInputEvent::MiddleButtonDown)
    x_state |= Button2Mask;
  if (modifiers & WebInputEvent::RightButtonDown)
    x_state |= Button3Mask;
  // TODO(piman@google.com): There are other modifiers, e.g. Num Lock, that
  // should be set (and Firefox does), but we didn't keep the information in
  // the WebKit event.
  return x_state;
}

static bool NPEventFromWebMouseEvent(const WebMouseEvent& event,
                                     Time timestamp,
                                     NPEvent *np_event) {
  np_event->xany.display = GDK_DISPLAY();
  // NOTE: Firefox keeps xany.serial and xany.window as 0.

  int modifier_state = GetXModifierState(event.modifiers);

  Window root = GDK_ROOT_WINDOW();
  switch (event.type) {
    case WebInputEvent::MouseMove: {
      np_event->type = MotionNotify;
      XMotionEvent &motion_event = np_event->xmotion;
      motion_event.root = root;
      motion_event.time = timestamp;
      motion_event.x = event.x;
      motion_event.y = event.y;
      motion_event.x_root = event.globalX;
      motion_event.y_root = event.globalY;
      motion_event.state = modifier_state;
      motion_event.is_hint = NotifyNormal;
      motion_event.same_screen = True;
      break;
    }
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseEnter: {
      if (event.type == WebInputEvent::MouseEnter) {
        np_event->type = EnterNotify;
      } else {
        np_event->type = LeaveNotify;
      }
      XCrossingEvent &crossing_event = np_event->xcrossing;
      crossing_event.root = root;
      crossing_event.time = timestamp;
      crossing_event.x = event.x;
      crossing_event.y = event.y;
      crossing_event.x_root = event.globalX;
      crossing_event.y_root = event.globalY;
      crossing_event.mode = -1;  // This is what Firefox sets it to.
      crossing_event.detail = NotifyDetailNone;
      crossing_event.same_screen = True;
      // TODO(piman@google.com): set this to the correct value. Firefox does. I
      // don't know where to get the information though, we get focus
      // notifications, but no unfocus.
      crossing_event.focus = 0;
      crossing_event.state = modifier_state;
      break;
    }
    case WebInputEvent::MouseUp:
    case WebInputEvent::MouseDown: {
      if (event.type == WebInputEvent::MouseDown) {
        np_event->type = ButtonPress;
      } else {
        np_event->type = ButtonRelease;
      }
      XButtonEvent &button_event = np_event->xbutton;
      button_event.root = root;
      button_event.time = timestamp;
      button_event.x = event.x;
      button_event.y = event.y;
      button_event.x_root = event.globalX;
      button_event.y_root = event.globalY;
      button_event.state = modifier_state;
      switch (event.button) {
        case WebMouseEvent::ButtonLeft:
          button_event.button = Button1;
          break;
        case WebMouseEvent::ButtonMiddle:
          button_event.button = Button2;
          break;
        case WebMouseEvent::ButtonRight:
          button_event.button = Button3;
          break;
        default:
          NOTREACHED();
      }
      button_event.same_screen = True;
      break;
    }
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

static bool NPEventFromWebKeyboardEvent(const WebKeyboardEvent& event,
                                        Time timestamp,
                                        NPEvent *np_event) {
  np_event->xany.display = GDK_DISPLAY();
  // NOTE: Firefox keeps xany.serial and xany.window as 0.

  switch (event.type) {
    case WebKeyboardEvent::KeyDown:
      np_event->type = KeyPress;
      break;
    case WebKeyboardEvent::KeyUp:
      np_event->type = KeyRelease;
      break;
    default:
      NOTREACHED();
      return false;
  }
  XKeyEvent &key_event = np_event->xkey;
  key_event.send_event = False;
  key_event.display = GDK_DISPLAY();
  // NOTE: Firefox keeps xany.serial and xany.window as 0.
  // TODO(piman@google.com): is this right for multiple screens ?
  key_event.root = DefaultRootWindow(key_event.display);
  key_event.time = timestamp;
  // NOTE: We don't have the correct information for x/y/x_root/y_root. Firefox
  // doesn't have it either, so we pass the same values.
  key_event.x = 0;
  key_event.y = 0;
  key_event.x_root = -1;
  key_event.y_root = -1;
  key_event.state = GetXModifierState(event.modifiers);
  key_event.keycode = event.nativeKeyCode;
  key_event.same_screen = True;
  return true;
}

static bool NPEventFromWebInputEvent(const WebInputEvent& event,
                                     Time timestamp,
                                     NPEvent* np_event) {
  switch (event.type) {
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseEnter:
    case WebInputEvent::MouseDown:
    case WebInputEvent::MouseUp:
      if (event.size < sizeof(WebMouseEvent)) {
        NOTREACHED();
        return false;
      }
      return NPEventFromWebMouseEvent(
          *static_cast<const WebMouseEvent*>(&event), timestamp, np_event);
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
      if (event.size < sizeof(WebKeyboardEvent)) {
        NOTREACHED();
        return false;
      }
      return NPEventFromWebKeyboardEvent(
          *static_cast<const WebKeyboardEvent*>(&event), timestamp, np_event);
    default:
      return false;
  }
}

bool WebPluginDelegateImpl::PlatformHandleInputEvent(
    const WebInputEvent& event, WebCursorInfo* cursor_info) {

  if (first_event_time_ < 0.0)
    first_event_time_ = event.timeStampSeconds;
  Time timestamp = static_cast<Time>(
      (event.timeStampSeconds - first_event_time_) * 1.0e3);
  NPEvent np_event = {0};
  if (!NPEventFromWebInputEvent(event, timestamp, &np_event)) {
    return false;
  }
  // See comment about PLUGIN_QUIRK_WINDOWLESS_NO_RIGHT_CLICK in constructor.
  if (windowless_ &&
      (quirks_ & PLUGIN_QUIRK_WINDOWLESS_NO_RIGHT_CLICK) &&
      (np_event.type == ButtonPress || np_event.type == ButtonRelease) &&
      (np_event.xbutton.button == Button3)) {
    return false;
  }

  bool ret = instance()->NPP_HandleEvent(&np_event) != 0;

  // Flash always returns false, even when the event is handled.
  ret = true;

#if 0
  if (event->event == WM_MOUSEMOVE) {
    // Snag a reference to the current cursor ASAP in case the plugin modified
    // it. There is a nasty race condition here with the multiprocess browser
    // as someone might be setting the cursor in the main process as well.
    *cursor = current_windowless_cursor_;
  }
#endif

  return ret;
}

}  // namespace npapi
}  // namespace webkit
