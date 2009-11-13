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
#include "base/scoped_ptr.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
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

// If we're compiling support for the QuickDraw drawing model, turn off GCC
// warnings about deprecated functions (since QuickDraw is a deprecated API).
// According to the GCC documentation, this can only be done per file, not
// pushed and popped like some options can be.
#ifndef NP_NO_QUICKDRAW
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

using webkit_glue::WebPlugin;
using webkit_glue::WebPluginDelegate;
using webkit_glue::WebPluginResourceClient;
using WebKit::WebCursorInfo;
using WebKit::WebKeyboardEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

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
    : windowless_needs_set_window_(true),
      // all Mac plugins are "windowless" in the Windows/X11 sense
      windowless_(true),
      plugin_(NULL),
      instance_(instance),
      parent_(containing_view),
      qd_world_(0),
      quirks_(0),
      null_event_factory_(this),
      waiting_to_die_(false),
      last_mouse_x_(0),
      last_mouse_y_(0),
      handle_event_depth_(0),
      user_gesture_message_posted_(this),
      user_gesture_msg_factory_(this) {
  memset(&window_, 0, sizeof(window_));
#ifndef NP_NO_QUICKDRAW
  memset(&qd_port_, 0, sizeof(qd_port_));
#endif
  instance->set_windowless(true);

  const WebPluginInfo& plugin_info = instance_->plugin_lib()->plugin_info();
  if (plugin_info.name.find(L"QuickTime") != std::wstring::npos) {
    // In some cases, QuickTime inexpicably negotiates the CoreGraphics drawing
    // model, but then proceeds as if it were using QuickDraw. Until we support
    // CoreAnimation, just ignore what QuickTime asks for.
    quirks_ |= PLUGIN_QUIRK_IGNORE_NEGOTIATED_DRAWING_MODEL;
  }
}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
#ifndef NP_NO_QUICKDRAW
  if (qd_port_.port) {
    DisposeGWorld(qd_port_.port);
    DisposeGWorld(qd_world_);
  }
#endif
}

void WebPluginDelegateImpl::PluginDestroyed() {
  FakePluginWindowTracker::SharedInstance()->RemoveFakeWindowForDelegate(
      this, reinterpret_cast<WindowRef>(cg_context_.window));

  if (instance_->event_model() == NPEventModelCarbon) {
    if (PluginDrawingModel() == NPDrawingModelQuickDraw) {
      // Tell the plugin it should stop drawing into the GWorld (which will go
      // away when the next idle event arrives).
      window_.x = 0;
      window_.y = 0;
      window_.width = 0;
      window_.height = 0;
      window_.clipRect.top = 0;
      window_.clipRect.left = 0;
      window_.clipRect.bottom = 0;
      window_.clipRect.right = 0;
      instance()->NPP_SetWindow(&window_);
      QDFlushPortBuffer(qd_port_.port, NULL);
    }
    DestroyInstance();
    // We have an idle event queued up to call us back in a few ms, so don't
    // actually delete this until it arrives.  Set |waiting_to_die_| so that
    // OnNullEvent knows to delete this rather than call into the now-destroyed
    // plugin.
    waiting_to_die_ = true;
  } else {
    DestroyInstance();
    delete this;
  }
}

void WebPluginDelegateImpl::PlatformInitialize() {
  // Don't set a NULL window handle on destroy for Mac plugins.  This matches
  // Safari and other Mac browsers (see PluginView::stop() in PluginView.cpp,
  // where code to do so is surrounded by an #ifdef that excludes Mac OS X, or
  // destroyPlugin in WebNetscapePluginView.mm, for examples).
  quirks_ |= PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY;

  // Create a stand-in for the browser window so that the plugin will have
  // a non-NULL WindowRef to which it can refer.
  FakePluginWindowTracker* window_tracker =
      FakePluginWindowTracker::SharedInstance();
  cg_context_.window = window_tracker->GenerateFakeWindowForDelegate(this);
  cg_context_.context = NULL;
  Rect window_bounds = { 0, 0, window_rect_.height(), window_rect_.width() };
  SetWindowBounds(reinterpret_cast<WindowRef>(cg_context_.window),
                  kWindowContentRgn, &window_bounds);

  switch (PluginDrawingModel()) {
#ifndef NP_NO_QUICKDRAW
    case NPDrawingModelQuickDraw:
      window_.window = &qd_port_;
      window_.type = NPWindowTypeDrawable;
      break;
#endif
    case NPDrawingModelCoreGraphics:
      window_.window = &cg_context_;
      window_.type = NPWindowTypeDrawable;
      break;
    default:
      NOTREACHED();
      break;
  }

  // If the plugin wants Carbon events, fire up a source of idle events.
  if (instance_->event_model() == NPEventModelCarbon) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        null_event_factory_.NewRunnableMethod(
            &WebPluginDelegateImpl::OnNullEvent), kPluginIdleThrottleDelayMs);
  }
  plugin_->SetWindow(NULL);
}

void WebPluginDelegateImpl::PlatformDestroyInstance() {
  // TODO(port): implement these after unforking.
}

void WebPluginDelegateImpl::UpdateContext(CGContextRef context) {
  // Flash on the Mac apparently caches the context from the struct it recieves
  // in NPP_SetWindow, and continue to use it even when the contents of the
  // struct have changed, so we need to call NPP_SetWindow again if the context
  // changes.
  if (context != cg_context_.context) {
    cg_context_.context = context;
#ifndef NP_NO_QUICKDRAW
    if (PluginDrawingModel() == NPDrawingModelQuickDraw) {
      if (qd_port_.port) {
        DisposeGWorld(qd_port_.port);
        DisposeGWorld(qd_world_);
        qd_port_.port = NULL;
        qd_world_ = NULL;
      }
      Rect window_bounds = {
        0, 0, window_rect_.height(), window_rect_.width()
      };
      // Create a GWorld pointing at the same bits as our CGContextRef
      NewGWorldFromPtr(&qd_world_, k32BGRAPixelFormat, &window_bounds,
          NULL, NULL, 0,
          static_cast<Ptr>(CGBitmapContextGetData(context)),
          static_cast<SInt32>(CGBitmapContextGetBytesPerRow(context)));
      // Create a GWorld for the plugin to paint into whenever it wants
      NewGWorld(&qd_port_.port, k32ARGBPixelFormat, &window_bounds,
          NULL, NULL, kNativeEndianPixMap);
      SetGWorld(qd_port_.port, NULL);
      // Fill with black
      ForeColor(blackColor);
      BackColor(whiteColor);
      PaintRect(&window_bounds);
    }
#endif
    WindowlessSetWindow(true);
  }
}

void WebPluginDelegateImpl::Paint(CGContextRef context, const gfx::Rect& rect) {
  DCHECK(windowless_);
  WindowlessPaint(context, rect);
}

void WebPluginDelegateImpl::Print(CGContextRef context) {
  // Disabling the call to NPP_Print as it causes a crash in
  // flash in some cases. In any case this does not work as expected
  // as the EMF meta file dc passed in needs to be created with the
  // the plugin window dc as its sibling dc and the window rect
  // in .01 mm units.
}

void WebPluginDelegateImpl::InstallMissingPlugin() {
  NOTIMPLEMENTED();
}

bool WebPluginDelegateImpl::WindowedCreatePlugin() {
  NOTREACHED();
  return false;
}

void WebPluginDelegateImpl::WindowedDestroyWindow() {
  NOTREACHED();
}

bool WebPluginDelegateImpl::WindowedReposition(const gfx::Rect& window_rect,
                                               const gfx::Rect& clip_rect) {
  NOTREACHED();
  return false;
}

void WebPluginDelegateImpl::WindowedSetWindow() {
  NOTREACHED();
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

  switch (PluginDrawingModel()) {
#ifndef NP_NO_QUICKDRAW
    case NPDrawingModelQuickDraw: {
      // Plugins using the QuickDraw drawing model do not restrict their
      // drawing to update events the way that CoreGraphics-based plugins
      // do.  When we are asked to paint, we therefore just copy from the
      // plugin's persistent offscreen GWorld into our shared memory bitmap
      // context.
      Rect window_bounds = {
        0, 0, window_rect_.height(), window_rect_.width()
      };
      PixMapHandle plugin_pixmap = GetGWorldPixMap(qd_port_.port);
      if (LockPixels(plugin_pixmap)) {
        PixMapHandle shared_pixmap = GetGWorldPixMap(qd_world_);
        if (LockPixels(shared_pixmap)) {
          SetGWorld(qd_world_, NULL);
          // Set foreground and background colors to avoid "colorizing" the
          // image.
          ForeColor(blackColor);
          BackColor(whiteColor);
          CopyBits(reinterpret_cast<BitMap*>(*plugin_pixmap),
                   reinterpret_cast<BitMap*>(*shared_pixmap),
                   &window_bounds, &window_bounds, srcCopy, NULL);
          UnlockPixels(shared_pixmap);
        }
        UnlockPixels(plugin_pixmap);
      }
      break;
    }
#endif
    case NPDrawingModelCoreGraphics: {
      CGContextSaveGState(context);
      switch (instance()->event_model()) {
        case NPEventModelCarbon: {
          NPEvent paint_event = { 0 };
          paint_event.what = updateEvt;
          paint_event.message = reinterpret_cast<uint32>(cg_context_.window);
          paint_event.when = TickCount();
          instance()->NPP_HandleEvent(&paint_event);
          break;
        }
        case NPEventModelCocoa: {
          NPCocoaEvent paint_event;
          memset(&paint_event, 0, sizeof(NPCocoaEvent));
          paint_event.type = NPCocoaEventDrawRect;
          paint_event.data.draw.context = context;
          paint_event.data.draw.x = damage_rect.x();
          paint_event.data.draw.y = damage_rect.y();
          paint_event.data.draw.width = damage_rect.width();
          paint_event.data.draw.height = damage_rect.height();
          instance()->NPP_HandleEvent(reinterpret_cast<NPEvent*>(&paint_event));
          break;
        }
      }
      CGContextRestoreGState(context);
    }
  }
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
  int old_width = window_bounds.right - window_bounds.left;
  int old_height = window_bounds.bottom - window_bounds.top;
  if (window_bounds.left != target_x ||
      window_bounds.top != target_y ||
      (new_width && new_width != old_width) ||
      (new_height && new_height != old_height)) {
    int height = new_height ? new_height : old_height;
    int width = new_width ? new_width : old_width;
    window_bounds.left = target_x;
    window_bounds.top = target_y;
    window_bounds.right = window_bounds.left + width;
    window_bounds.bottom = window_bounds.top + height;
    SetWindowBounds(window, kWindowContentRgn, &window_bounds);
  }
}

void WebPluginDelegateImpl::WindowlessSetWindow(bool force_set_window) {
  if (!instance())
    return;

  window_.clipRect.top = 0;
  window_.clipRect.left = 0;
  window_.clipRect.bottom = window_rect_.height();
  window_.clipRect.right = window_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = 0;
  window_.y = 0;

  UpdateDummyWindowBoundsWithOffset(
      reinterpret_cast<WindowRef>(cg_context_.window), window_rect_.x(),
      window_rect_.y(), window_rect_.width(), window_rect_.height());

  NPError err = instance()->NPP_SetWindow(&window_);

  DCHECK(err == NPERR_NO_ERROR);
}

void WebPluginDelegateImpl::SetFocus() {
  switch (instance()->event_model()) {
    case NPEventModelCarbon: {
      NPEvent focus_event = { 0 };
      focus_event.what = NPEventType_GetFocusEvent;
      focus_event.when = TickCount();
      instance()->NPP_HandleEvent(&focus_event);
      break;
    }
    case NPEventModelCocoa: {
      NPCocoaEvent focus_event;
      memset(&focus_event, 0, sizeof(focus_event));
      focus_event.type = NPCocoaEventFocusChanged;
      focus_event.data.focus.hasFocus = true;
      instance()->NPP_HandleEvent(reinterpret_cast<NPEvent*>(&focus_event));
      break;
    }
  }
}

int WebPluginDelegateImpl::PluginDrawingModel() {
  if (quirks_ & PLUGIN_QUIRK_IGNORE_NEGOTIATED_DRAWING_MODEL)
    return NPDrawingModelQuickDraw;
  return instance()->drawing_model();
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
    default:
      NOTIMPLEMENTED();
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


static bool NPCocoaEventFromWebMouseEvent(const WebMouseEvent& event,
                                          NPCocoaEvent *np_cocoa_event) {
  np_cocoa_event->data.mouse.pluginX = event.x;
  np_cocoa_event->data.mouse.pluginY = event.y;

  if (event.modifiers & WebInputEvent::ControlKey)
    np_cocoa_event->data.mouse.modifierFlags |= controlKey;
  if (event.modifiers & WebInputEvent::ShiftKey)
    np_cocoa_event->data.mouse.modifierFlags |= shiftKey;

  np_cocoa_event->data.mouse.clickCount = event.clickCount;
  switch (event.button) {
    case WebMouseEvent::ButtonLeft:
      np_cocoa_event->data.mouse.buttonNumber = 0;
      break;
    case WebMouseEvent::ButtonMiddle:
      np_cocoa_event->data.mouse.buttonNumber = 2;
      break;
    case WebMouseEvent::ButtonRight:
      np_cocoa_event->data.mouse.buttonNumber = 1;
      break;
    default:
      np_cocoa_event->data.mouse.buttonNumber = event.button;
      break;
  }
  switch (event.type) {
    case WebInputEvent::MouseDown:
      np_cocoa_event->type = NPCocoaEventMouseDown;
      return true;
    case WebInputEvent::MouseUp:
      np_cocoa_event->type = NPCocoaEventMouseUp;
      return true;
    case WebInputEvent::MouseMove:
      np_cocoa_event->type = NPCocoaEventMouseMoved;
      return true;
    case WebInputEvent::MouseEnter:
      np_cocoa_event->type = NPCocoaEventMouseEntered;
      return true;
    case WebInputEvent::MouseLeave:
      np_cocoa_event->type = NPCocoaEventMouseExited;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool NPCocoaEventFromWebMouseWheelEvent(const WebMouseWheelEvent& event,
                                               NPCocoaEvent *np_cocoa_event) {
  np_cocoa_event->type = NPCocoaEventScrollWheel;
  np_cocoa_event->data.mouse.pluginX = event.x;
  np_cocoa_event->data.mouse.pluginY = event.y;
  if (event.modifiers & WebInputEvent::ControlKey)
    np_cocoa_event->data.mouse.modifierFlags |= controlKey;
  if (event.modifiers & WebInputEvent::ShiftKey)
    np_cocoa_event->data.mouse.modifierFlags |= shiftKey;
  np_cocoa_event->data.mouse.deltaX = event.deltaX;
  np_cocoa_event->data.mouse.deltaY = event.deltaY;
  return true;
}

static bool NPCocoaEventFromWebKeyboardEvent(const WebKeyboardEvent& event,
                                             NPCocoaEvent *np_cocoa_event) {
  np_cocoa_event->data.key.keyCode = event.nativeKeyCode;

  if (event.modifiers & WebInputEvent::ControlKey)
    np_cocoa_event->data.key.modifierFlags |= controlKey;
  if (event.modifiers & WebInputEvent::ShiftKey)
    np_cocoa_event->data.key.modifierFlags |= shiftKey;
  if (event.modifiers & WebInputEvent::AltKey)
    np_cocoa_event->data.key.modifierFlags |= cmdKey;
  if (event.modifiers & WebInputEvent::MetaKey)
    np_cocoa_event->data.key.modifierFlags |= optionKey;

  if (event.modifiers & WebInputEvent::IsAutoRepeat)
    np_cocoa_event->data.key.isARepeat = true;

  switch (event.type) {
    case WebInputEvent::KeyDown:
      np_cocoa_event->type = NPCocoaEventKeyDown;
      return true;
    case WebInputEvent::KeyUp:
      np_cocoa_event->type = NPCocoaEventKeyUp;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool NPCocoaEventFromWebInputEvent(const WebInputEvent& event,
                                          NPCocoaEvent *np_cocoa_event) {
  memset(np_cocoa_event, 0, sizeof(NPCocoaEvent));
  if (event.type == WebInputEvent::MouseWheel) {
    return NPCocoaEventFromWebMouseWheelEvent(
        *static_cast<const WebMouseWheelEvent*>(&event), np_cocoa_event);
  } else if (WebInputEventIsWebMouseEvent(event)) {
    return NPCocoaEventFromWebMouseEvent(
        *static_cast<const WebMouseEvent*>(&event), np_cocoa_event);
  } else if (WebInputEventIsWebKeyboardEvent(event)) {
    return NPCocoaEventFromWebKeyboardEvent(
        *static_cast<const WebKeyboardEvent*>(&event), np_cocoa_event);
  }
  DLOG(WARNING) << "unknown event type " << event.type;
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

#ifndef NP_NO_CARBON
  NPEvent np_event = {0};
  if (!NPEventFromWebInputEvent(event, &np_event)) {
    LOG(WARNING) << "NPEventFromWebInputEvent failed";
    return false;
  }
  np_event.when = TickCount();
  if (np_event.what == nullEvent) {
    last_mouse_x_ = np_event.where.h;
    last_mouse_y_ = np_event.where.v;
    if (instance()->event_model() == NPEventModelCarbon)
      return true;  // Let the recurring task actually send the event.
  } else {
    // If this is a mouse event, we need to make sure our dummy window
    // has the correct location before we send the event to the plugin,
    // so that any coordinate conversion the plugin does will work out.
    if (WebInputEventIsWebMouseEvent(event)) {
      const WebMouseEvent* mouse_event =
          static_cast<const WebMouseEvent*>(&event);
      UpdateWindowLocation(reinterpret_cast<WindowRef>(cg_context_.window),
                           *mouse_event);
    }
  }
#endif

  bool ret = false;
  switch (PluginDrawingModel()) {
#ifndef NP_NO_QUICKDRAW
    case NPDrawingModelQuickDraw:
      SetGWorld(qd_port_.port, NULL);
      ret = instance()->NPP_HandleEvent(&np_event) != 0;
      break;
#endif
    case NPDrawingModelCoreGraphics:
      CGContextSaveGState(cg_context_.context);
      switch (instance()->event_model()) {
#ifndef NP_NO_CARBON
        case NPEventModelCarbon:
          // Send the event to the plugin.
          ret = instance()->NPP_HandleEvent(&np_event) != 0;
          break;
#endif
        case NPEventModelCocoa: {
          NPCocoaEvent np_cocoa_event;
          if (!NPCocoaEventFromWebInputEvent(event, &np_cocoa_event)) {
            LOG(WARNING) << "NPCocoaEventFromWebInputEvent failed";
            return false;
          }
          ret = instance()->NPP_HandleEvent(
              reinterpret_cast<NPEvent*>(&np_cocoa_event)) != 0;
          break;
        }
      }
      CGContextRestoreGState(cg_context_.context);
      break;
  }
  return ret;
}

void WebPluginDelegateImpl::OnNullEvent() {
  if (waiting_to_die_) {
    // if |waiting_to_die_| is set, it means that the plugin has been destroyed,
    // and the WebPluginDelegateImpl object was just waiting for this callback
    // to arrive.  Delete this and return to the message loop.
    delete this;
    return;
  }
#ifndef NP_NO_CARBON
  if (!webkit_glue::IsPluginRunningInRendererProcess()) {
    switch (instance_->event_model()) {
      case NPEventModelCarbon:
        // If the plugin is running in a subprocess, drain any pending system
        // events so that the plugin's event handlers will get called on any
        // windows it has created.  Filter out activate/deactivate events on
        // the fake browser window, but pass everything else through.
        EventRecord event;
        while (GetNextEvent(everyEvent, &event)) {
          if (event.what == activateEvt && cg_context_.window &&
              reinterpret_cast<void *>(event.message) != cg_context_.window)
            continue;
          instance()->NPP_HandleEvent(&event);
        }
        break;
    }
  }

  if (instance_->event_model() == NPEventModelCarbon) {
    // Send an idle event so that the plugin can do background work
    NPEvent np_event = {0};
    np_event.what = nullEvent;
    np_event.when = TickCount();
    np_event.modifiers = GetCurrentKeyModifiers();
    if (!Button())
      np_event.modifiers |= btnState;
    np_event.where.h = last_mouse_x_;
    np_event.where.v = last_mouse_y_;
    instance()->NPP_HandleEvent(&np_event);
  }
#endif

#ifndef NP_NO_QUICKDRAW
  // Quickdraw-based plugins can draw at any time, so tell the renderer to
  // repaint.
  // TODO: only do this if the contents of the offscreen GWorld has changed,
  // so as not to spam the renderer with an unchanging image.
  if (PluginDrawingModel() == NPDrawingModelQuickDraw)
    instance()->webplugin()->Invalidate();
#endif

#ifndef NP_NO_CARBON
  if (instance_->event_model() == NPEventModelCarbon) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        null_event_factory_.NewRunnableMethod(
            &WebPluginDelegateImpl::OnNullEvent),
        kPluginIdleThrottleDelayMs);
  }
#endif
}
