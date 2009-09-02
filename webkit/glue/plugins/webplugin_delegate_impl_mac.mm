// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "webkit/glue/plugins/webplugin_delegate_impl.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/default_plugin/plugin_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/fake_plugin_window_tracker_mac.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "webkit/glue/webkit_glue.h"

using webkit_glue::WebPlugin;
using webkit_glue::WebPluginDelegate;
using webkit_glue::WebPluginResourceClient;
using WebKit::WebCursorInfo;
using WebKit::WebKeyboardEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;

// Important implementation notes: The Mac definition of NPAPI, particularly
// the distinction between windowed and windowless modes, differs from the
// Windows and Linux definitions.  Most of those differences are
// accomodated by the WebPluginDelegate class.

namespace {

// The fastest we are willing to process idle events for plugins.
// Some can easily exceed the limits of our CPU if we don't throttle them.
// The throttle has been chosen by using the same value as Apple's WebKit port.
//
// We'd like to make the throttle delay variable, based on the amount of
// time currently required to paint plugins.  There isn't a good
// way to count the time spent in aggregate plugin painting, however, so
// this seems to work well enough.
const int kPluginIdleThrottleDelayMs = 20;  // 20ms (50Hz)

// The most recently seen offset between global and local coordinates.  We use
// this to keep the placeholder Carbon WindowRef's origin in sync with the
// actual browser window, without having to pass that geometry over IPC.  If we
// end up needing to interpose on Carbon APIs in the plugin process (in order
// to simulate window activation, for example), this could be replaced by
// interposing on GlobalToLocal and/or LocalToGlobal (see related TODO comments
// below in WebPluginDelegateImpl::OnNullEvent()).

int g_current_x_offset = 0;
int g_current_y_offset = 0;

}  // namespace

WebPluginDelegateImpl::WebPluginDelegateImpl(
    gfx::PluginWindowHandle containing_view,
    NPAPI::PluginInstance *instance)
    : parent_(containing_view),
      instance_(instance),
      quirks_(0),
      plugin_(NULL),
      // all Mac plugins are "windowless" in the Windows/X11 sense
      windowless_(true),
      windowless_needs_set_window_(true),
      handle_event_depth_(0),
      user_gesture_message_posted_(this),
      user_gesture_msg_factory_(this),
      null_event_factory_(this),
      last_mouse_x_(0),
      last_mouse_y_(0) {
  memset(&window_, 0, sizeof(window_));
}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
  FakePluginWindowTracker::SharedInstance()->RemoveFakeWindowForDelegate(
      this, cg_context_.window);
  DestroyInstance();
}

void WebPluginDelegateImpl::PluginDestroyed() {
  delete this;
}

void WebPluginDelegateImpl::PlatformInitialize() {
  FakePluginWindowTracker* window_tracker =
      FakePluginWindowTracker::SharedInstance();
  cg_context_.window = window_tracker->GenerateFakeWindowForDelegate(this);
  cg_context_.context = NULL;
  Rect window_bounds = { 0, 0, window_rect_.height(), window_rect_.width() };
  SetWindowBounds(cg_context_.window, kWindowContentRgn, &window_bounds);
  window_.window = &cg_context_;
  window_.type = NPWindowTypeWindow;
  instance_->set_window_handle(cg_context_.window);
  instance_->set_windowless(true);
  windowless_ = true;
  plugin_->SetWindow(NULL);

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      null_event_factory_.NewRunnableMethod(
          &WebPluginDelegateImpl::OnNullEvent),
      kPluginIdleThrottleDelayMs);
}

void WebPluginDelegateImpl::PlatformDestroyInstance() {
}

void WebPluginDelegateImpl::UpdateContext(CGContextRef context) {
  // Flash on the Mac apparently caches the context from the struct it recieves
  // in NPP_SetWindow, and continue to use it even when the contents of the
  // struct have changed, so we need to call NPP_SetWindow again if the context
  // changes.
  if (context != cg_context_.context) {
    cg_context_.context = context;
    WindowlessSetWindow(true);
  }
}

void WebPluginDelegateImpl::Paint(CGContextRef context, const gfx::Rect& rect) {
  DCHECK(windowless_);
  WindowlessPaint(context, rect);
}

void WebPluginDelegateImpl::Print(CGContextRef context) {
}

void WebPluginDelegateImpl::InstallMissingPlugin() {
}

void WebPluginDelegateImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  // Only resend to the instance if the geometry has changed.
  if (window_rect == window_rect_ && clip_rect == clip_rect_)
    return;

  // We will inform the instance of this change when we call NPP_SetWindow.
  clip_rect_ = clip_rect;

  if (window_rect_ != window_rect) {
    window_rect_ = window_rect;

    WindowlessSetWindow(true);
  }
}

void WebPluginDelegateImpl::WindowlessPaint(gfx::NativeDrawingContext context,
                                            const gfx::Rect& damage_rect) {
  // If we somehow get a paint before we've set up the plugin window, bail.
  if (!cg_context_.context)
    return;
  DCHECK(cg_context_.context == context);

  static StatsRate plugin_paint("Plugin.Paint");
  StatsScope<StatsRate> scope(plugin_paint);

  // We save and restore the NSGraphicsContext state in case the plugin uses
  // Cocoa drawing.
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:[NSGraphicsContext
                                        graphicsContextWithGraphicsPort:context
                                        flipped:YES]];
  CGContextSaveGState(context);

  NPEvent paint_event;
  paint_event.what = updateEvt;
  paint_event.message = reinterpret_cast<uint32>(cg_context_.window);
  paint_event.when = TickCount();
  paint_event.where.h = 0;
  paint_event.where.v = 0;
  paint_event.modifiers = 0;
  instance()->NPP_HandleEvent(&paint_event);

  CGContextRestoreGState(context);
  [NSGraphicsContext restoreGraphicsState];
}

// Moves our dummy window to the given offset relative to the last known
// location of the real renderer window's content view.
// If new_width or new_height is non-zero, the window size (content region)
// will be updated accordingly; if they are zero, the existing size will be
// preserved.
static void UpdateDummyWindowBoundsWithOffset(WindowRef window,
                                              int x_offset, int y_offset,
                                              int new_width, int new_height) {
  int target_x = g_current_x_offset + x_offset;
  int target_y = g_current_y_offset + y_offset;
  Rect window_bounds;
  GetWindowBounds(window, kWindowContentRgn, &window_bounds);
  if ((window_bounds.left != target_x) ||
      (window_bounds.top != target_y)) {
    int height = new_height ? new_height
                            : window_bounds.bottom - window_bounds.top;
    int width = new_width ? new_width
                          : window_bounds.right - window_bounds.left;
    window_bounds.left = target_x;
    window_bounds.top = target_y;
    window_bounds.right = window_bounds.left + width;
    window_bounds.bottom = window_bounds.top + height;
    SetWindowBounds(window, kWindowContentRgn, &window_bounds);
  }
}

void WebPluginDelegateImpl::WindowedSetWindow() {
  NOTREACHED();
}

bool WebPluginDelegateImpl::WindowedReposition(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  NOTREACHED();
  return false;
}

bool WebPluginDelegateImpl::WindowedCreatePlugin() {
  return true;
}

void WebPluginDelegateImpl::WindowlessSetWindow(bool force_set_window) {
  if (!instance())
    return;

  if (window_rect_.IsEmpty())  // wait for geometry to be set.
    return;

  window_.clipRect.top = 0;
  window_.clipRect.left = 0;
  window_.clipRect.bottom = window_rect_.height();
  window_.clipRect.right = window_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = 0;
  window_.y = 0;

  UpdateDummyWindowBoundsWithOffset(cg_context_.window, window_rect_.x(),
                                    window_rect_.y(), window_rect_.width(),
                                    window_rect_.height());

  NPError err = instance()->NPP_SetWindow(&window_);
  DCHECK(err == NPERR_NO_ERROR);
}

void WebPluginDelegateImpl::SetFocus() {
  NPEvent focus_event = { 0 };
  focus_event.what = NPEventType_GetFocusEvent;
  focus_event.when = TickCount();
  instance()->NPP_HandleEvent(&focus_event);
}

static bool WebInputEventIsWebMouseEvent(const WebInputEvent& event) {
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
      return true;
    default:
      return false;
  }
}

static bool WebInputEventIsWebKeyboardEvent(const WebInputEvent& event) {
  switch (event.type) {
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
      if (event.size < sizeof(WebKeyboardEvent)) {
        NOTREACHED();
        return false;
      }
      return true;
    default:
      return false;
  }
}

static bool NPEventFromWebMouseEvent(const WebMouseEvent& event,
                                     NPEvent *np_event) {
  np_event->where.h = event.globalX;
  np_event->where.v = event.globalY;

  if (event.modifiers & WebInputEvent::ControlKey)
    np_event->modifiers |= controlKey;
  if (event.modifiers & WebInputEvent::ShiftKey)
    np_event->modifiers |= shiftKey;

  // default to "button up"; override this for mouse down events below.
  np_event->modifiers |= btnState;

  switch (event.button) {
    case WebMouseEvent::ButtonLeft:
      break;
    case WebMouseEvent::ButtonMiddle:
      np_event->modifiers |= cmdKey;
      break;
    case WebMouseEvent::ButtonRight:
      np_event->modifiers |= controlKey;
      break;
  }
  switch (event.type) {
    case WebInputEvent::MouseMove:
      np_event->what = nullEvent;
      return true;
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseEnter:
      np_event->what = NPEventType_AdjustCursorEvent;
      return true;
    case WebInputEvent::MouseDown:
      np_event->modifiers &= ~btnState;
      np_event->what = mouseDown;
      return true;
    case WebInputEvent::MouseUp:
      np_event->what = mouseUp;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool NPEventFromWebKeyboardEvent(const WebKeyboardEvent& event,
                                        NPEvent *np_event) {
  // TODO: figure out how to handle Unicode input to plugins, if that's
  // even possible in the NPAPI Carbon event model.
  np_event->message = (event.nativeKeyCode << 8) & keyCodeMask;
  np_event->message |= event.text[0] & charCodeMask;
  np_event->modifiers |= btnState;
  if (event.modifiers & WebInputEvent::ControlKey)
    np_event->modifiers |= controlKey;
  if (event.modifiers & WebInputEvent::ShiftKey)
    np_event->modifiers |= shiftKey;
  if (event.modifiers & WebInputEvent::AltKey)
    np_event->modifiers |= cmdKey;
  if (event.modifiers & WebInputEvent::MetaKey)
    np_event->modifiers |= optionKey;

  switch (event.type) {
    case WebInputEvent::KeyDown:
      if (event.modifiers & WebInputEvent::IsAutoRepeat)
        np_event->what = autoKey;
      else
        np_event->what = keyDown;
      return true;
    case WebInputEvent::KeyUp:
      np_event->what = keyUp;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool NPEventFromWebInputEvent(const WebInputEvent& event,
                                     NPEvent* np_event) {
  if (WebInputEventIsWebMouseEvent(event)) {
    return NPEventFromWebMouseEvent(*static_cast<const WebMouseEvent*>(&event),
                                    np_event);
  } else if (WebInputEventIsWebKeyboardEvent(event)) {
    return NPEventFromWebKeyboardEvent(
        *static_cast<const WebKeyboardEvent*>(&event), np_event);
  }
  DLOG(WARNING) << "unknown event type" << event.type;
  return false;
}

static void UpdateWindowLocation(WindowRef window, const WebMouseEvent& event) {
  // TODO: figure out where the vertical offset of 22 comes from (and if 22 is
  // exactly right) and replace with an appropriate calculation. It feels like
  // window structure or the menu bar, but neither should be involved here.
  g_current_x_offset = event.globalX - event.windowX;
  g_current_y_offset = event.globalY - event.windowY + 22;

  UpdateDummyWindowBoundsWithOffset(window, event.windowX - event.x,
                                    event.windowY - event.y, 0, 0);
}

bool WebPluginDelegateImpl::HandleInputEvent(const WebInputEvent& event,
                                             WebCursorInfo* cursor) {
  // If we somehow get an event before we've set up the plugin window, bail.
  if (!cg_context_.context)
    return false;
  DCHECK(windowless_) << "events should only be received in windowless mode";
  DCHECK(cursor != NULL);

  NPEvent np_event = {0};
  if (!NPEventFromWebInputEvent(event, &np_event)) {
    return false;
  }
  np_event.when = TickCount();
  if (np_event.what == nullEvent) {
    last_mouse_x_ = np_event.where.h;
    last_mouse_y_ = np_event.where.v;
    return true;  // Let the recurring task actually send the event.
  }

  // If this is a mouse event, we need to make sure our dummy window has the
  // correct location before we send the event to the plugin, so that any
  // coordinate conversion the plugin does will work out.
  if (WebInputEventIsWebMouseEvent(event)) {
    const WebMouseEvent* mouse_event =
        static_cast<const WebMouseEvent*>(&event);
    UpdateWindowLocation(cg_context_.window, *mouse_event);
  }
  CGContextSaveGState(cg_context_.context);
  bool ret = instance()->NPP_HandleEvent(&np_event) != 0;
  CGContextRestoreGState(cg_context_.context);
  return ret;
}

void WebPluginDelegateImpl::OnNullEvent() {
  NPEvent np_event = {0};
  np_event.what = nullEvent;
  np_event.when = TickCount();
  np_event.modifiers = GetCurrentKeyModifiers();
  if (!Button())
    np_event.modifiers |= btnState;
  np_event.where.h = last_mouse_x_;
  np_event.where.v = last_mouse_y_;
  instance()->NPP_HandleEvent(&np_event);

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      null_event_factory_.NewRunnableMethod(
          &WebPluginDelegateImpl::OnNullEvent),
      kPluginIdleThrottleDelayMs);
}
