// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "webkit/glue/plugins/webplugin_delegate_impl.h"

#include <string>
#include <unistd.h>
#include <vector>

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "webkit/default_plugin/plugin_impl.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/coregraphics_private_symbols_mac.h"
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

base::LazyInstance<std::set<WebPluginDelegateImpl*> > g_active_delegates(
    base::LINKER_INITIALIZED);

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
      quirks_(0),
      null_event_factory_(this),
      waiting_to_die_(false),
      last_window_x_offset_(0),
      last_window_y_offset_(0),
      last_mouse_x_(0),
      last_mouse_y_(0),
      have_focus_(false),
      focus_notifier_(NULL),
      handle_event_depth_(0),
      user_gesture_message_posted_(this),
      user_gesture_msg_factory_(this) {
  memset(&window_, 0, sizeof(window_));
#ifndef NP_NO_CARBON
  memset(&cg_context_, 0, sizeof(cg_context_));
#endif
#ifndef NP_NO_QUICKDRAW
  memset(&qd_port_, 0, sizeof(qd_port_));
#endif
  instance->set_windowless(true);

  std::set<WebPluginDelegateImpl*>* delegates = g_active_delegates.Pointer();
  delegates->insert(this);
}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
  std::set<WebPluginDelegateImpl*>* delegates = g_active_delegates.Pointer();
  delegates->erase(this);
#ifndef NP_NO_CARBON
  if (cg_context_.window) {
    FakePluginWindowTracker::SharedInstance()->RemoveFakeWindowForDelegate(
        this, reinterpret_cast<WindowRef>(cg_context_.window));
  }
#endif
}

void WebPluginDelegateImpl::PluginDestroyed() {
  if (instance()->event_model() == NPEventModelCarbon) {
    if (instance()->drawing_model() == NPDrawingModelQuickDraw) {
      // Tell the plugin it should stop drawing into the window (which will go
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

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
    // Create a stand-in for the browser window so that the plugin will have
    // a non-NULL WindowRef to which it can refer.
    FakePluginWindowTracker* window_tracker =
        FakePluginWindowTracker::SharedInstance();
    cg_context_.window = window_tracker->GenerateFakeWindowForDelegate(this);
    cg_context_.context = NULL;
    Rect window_bounds = { 0, 0, window_rect_.height(), window_rect_.width() };
    SetWindowBounds(reinterpret_cast<WindowRef>(cg_context_.window),
                    kWindowContentRgn, &window_bounds);
    qd_port_.port =
        GetWindowPort(reinterpret_cast<WindowRef>(cg_context_.window));
  }
#endif

  switch (instance()->drawing_model()) {
#ifndef NP_NO_QUICKDRAW
    case NPDrawingModelQuickDraw:
      window_.window = &qd_port_;
      window_.type = NPWindowTypeDrawable;
      break;
#endif
    case NPDrawingModelCoreGraphics:
#ifndef NP_NO_CARBON
      if (instance()->event_model() == NPEventModelCarbon)
        window_.window = &cg_context_;
#endif
      window_.type = NPWindowTypeDrawable;
      break;
    default:
      NOTREACHED();
      break;
  }

#ifndef NP_NO_CARBON
  // If the plugin wants Carbon events, fire up a source of idle events.
  if (instance()->event_model() == NPEventModelCarbon) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        null_event_factory_.NewRunnableMethod(
            &WebPluginDelegateImpl::OnNullEvent), kPluginIdleThrottleDelayMs);
  }
#endif
  plugin_->SetWindow(NULL);
}

void WebPluginDelegateImpl::PlatformDestroyInstance() {
  // TODO(port): implement these after unforking.
}

void WebPluginDelegateImpl::UpdateContext(CGContextRef context) {
#ifndef NP_NO_CARBON
  // Flash on the Mac apparently caches the context from the struct it receives
  // in NPP_SetWindow, and continues to use it even when the contents of the
  // struct have changed, so we need to call NPP_SetWindow again if the context
  // changes.
  if (instance()->event_model() == NPEventModelCarbon &&
      context != cg_context_.context) {
    cg_context_.context = context;
    WindowlessSetWindow(true);
  }
#endif
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
#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
    // If we somehow get a paint before we've set up the plugin window, bail.
    if (!cg_context_.context)
      return;
    DCHECK(cg_context_.context == context);
  }
#endif

  static StatsRate plugin_paint("Plugin.Paint");
  StatsScope<StatsRate> scope(plugin_paint);

  switch (instance()->drawing_model()) {
#ifndef NP_NO_QUICKDRAW
    case NPDrawingModelQuickDraw: {
      // Plugins using the QuickDraw drawing model do not restrict their
      // drawing to update events the way that CoreGraphics-based plugins
      // do.  When we are asked to paint, we therefore just copy from the
      // plugin's hidden window into our shared memory bitmap context.
      CGRect window_bounds = CGRectMake(0, 0,
                                        window_rect_.width(),
                                        window_rect_.height());
      CGWindowID window_id = HIWindowGetCGWindowID(
          reinterpret_cast<WindowRef>(cg_context_.window));
      CGContextSaveGState(context);
      CGContextTranslateCTM(context, 0, window_rect_.height());
      CGContextScaleCTM(context, 1.0, -1.0);
      CGContextCopyWindowCaptureContentsToRect(context, window_bounds,
                                               _CGSDefaultConnection(),
                                               window_id, 0);
      CGContextRestoreGState(context);
    }
#endif
    case NPDrawingModelCoreGraphics: {
      CGContextSaveGState(context);
      switch (instance()->event_model()) {
#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
          NPEvent paint_event = { 0 };
          paint_event.what = updateEvt;
          paint_event.message = reinterpret_cast<uint32>(cg_context_.window);
          paint_event.when = TickCount();
          instance()->NPP_HandleEvent(&paint_event);
          break;
        }
#endif
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

void WebPluginDelegateImpl::WindowlessSetWindow(bool force_set_window) {
  if (!instance())
    return;

  int y_offset = 0;
#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon &&
      instance()->drawing_model() == NPDrawingModelCoreGraphics) {
    // Get the dummy window structure height; we're pretenting the plugin takes
    // up the whole (dummy) window, but the clip rect and x/y are relative to
    // the full window region, not just the content region.
    Rect titlebar_bounds;
    WindowRef window = reinterpret_cast<WindowRef>(cg_context_.window);
    GetWindowBounds(window, kWindowTitleBarRgn, &titlebar_bounds);
    y_offset = titlebar_bounds.bottom - titlebar_bounds.top;
  }
#endif
  // It's not clear what we should do in the QD case; Safari always seems to use
  // 0, whereas Firefox uses the offset in the window and passes -offset as
  // port_y in the NP_Port structure. Since the port we are using corresponds
  // directly to the context, not the window, we need to use 0 for now, but
  // that may not work once we get events working.

  window_.x = 0;
  window_.y = y_offset;
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.clipRect.left = window_.x;
  window_.clipRect.top = window_.y;
  window_.clipRect.right = window_.clipRect.left + window_.width;
  window_.clipRect.bottom = window_.clipRect.top + window_.height;

  UpdateDummyWindowBoundsWithOffset(window_rect_.x(), window_rect_.y(),
                                    window_rect_.width(),
                                    window_rect_.height());

  NPError err = instance()->NPP_SetWindow(&window_);

  DCHECK(err == NPERR_NO_ERROR);
}

std::set<WebPluginDelegateImpl*> WebPluginDelegateImpl::GetActiveDelegates() {
  std::set<WebPluginDelegateImpl*>* delegates = g_active_delegates.Pointer();
  return *delegates;
}

void WebPluginDelegateImpl::FocusNotify(WebPluginDelegateImpl* delegate) {
  if (waiting_to_die_)
    return;

  have_focus_ = (delegate == this);

  switch (instance()->event_model()) {
    case NPEventModelCarbon: {
      NPEvent focus_event = { 0 };
      if (have_focus_)
        focus_event.what = NPEventType_GetFocusEvent;
      else
        focus_event.what = NPEventType_LoseFocusEvent;
      focus_event.when = TickCount();
      instance()->NPP_HandleEvent(&focus_event);
      break;
    }
    case NPEventModelCocoa: {
      NPCocoaEvent focus_event;
      memset(&focus_event, 0, sizeof(focus_event));
      focus_event.type = NPCocoaEventFocusChanged;
      focus_event.data.focus.hasFocus = have_focus_;
      instance()->NPP_HandleEvent(reinterpret_cast<NPEvent*>(&focus_event));
      break;
    }
  }
}

void WebPluginDelegateImpl::SetFocus() {
  if (focus_notifier_)
    focus_notifier_(this);
  else
    FocusNotify(this);
}

void WebPluginDelegateImpl::UpdatePluginLocation(const WebMouseEvent& event) {
  instance()->set_plugin_origin(gfx::Point(event.globalX - event.x,
                                           event.globalY - event.y));

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
    last_window_x_offset_ = event.globalX - event.windowX;
    last_window_y_offset_ = event.globalY - event.windowY;
    last_mouse_x_ = event.globalX;
    last_mouse_y_ = event.globalY;

    UpdateDummyWindowBoundsWithOffset(event.windowX - event.x,
                                      event.windowY - event.y, 0, 0);
  }
#endif
}

void WebPluginDelegateImpl::UpdateDummyWindowBoundsWithOffset(
    int x_offset, int y_offset, int new_width, int new_height) {
  if (instance()->event_model() == NPEventModelCocoa)
    return;

  int target_x = last_window_x_offset_ + x_offset;
  int target_y = last_window_y_offset_ + y_offset;
  WindowRef window = reinterpret_cast<WindowRef>(cg_context_.window);
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

#ifndef NP_NO_CARBON
static NSInteger CarbonModifiersFromWebEvent(const WebInputEvent& event) {
  NSInteger modifiers = 0;
  if (event.modifiers & WebInputEvent::ControlKey)
    modifiers |= controlKey;
  if (event.modifiers & WebInputEvent::ShiftKey)
    modifiers |= shiftKey;
  if (event.modifiers & WebInputEvent::AltKey)
    modifiers |= optionKey;
  if (event.modifiers & WebInputEvent::MetaKey)
    modifiers |= cmdKey;
  return modifiers;
}

static bool NPEventFromWebMouseEvent(const WebMouseEvent& event,
                                     NPEvent *np_event) {
  np_event->where.h = event.globalX;
  np_event->where.v = event.globalY;

  np_event->modifiers |= CarbonModifiersFromWebEvent(event);

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
  np_event->modifiers |= CarbonModifiersFromWebEvent(event);

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
  np_event->when = TickCount();
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
#endif  // !NP_NO_CARBON

static NSInteger CocoaModifiersFromWebEvent(const WebInputEvent& event) {
  NSInteger modifiers = 0;
  if (event.modifiers & WebInputEvent::ControlKey)
    modifiers |= NSControlKeyMask;
  if (event.modifiers & WebInputEvent::ShiftKey)
    modifiers |= NSShiftKeyMask;
  if (event.modifiers & WebInputEvent::AltKey)
    modifiers |= NSAlternateKeyMask;
  if (event.modifiers & WebInputEvent::MetaKey)
    modifiers |= NSCommandKeyMask;
  return modifiers;
}

static bool NPCocoaEventFromWebMouseEvent(const WebMouseEvent& event,
                                          NPCocoaEvent *np_cocoa_event) {
  np_cocoa_event->data.mouse.pluginX = event.x;
  np_cocoa_event->data.mouse.pluginY = event.y;
  np_cocoa_event->data.mouse.modifierFlags |= CocoaModifiersFromWebEvent(event);
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
  np_cocoa_event->data.mouse.modifierFlags |= CocoaModifiersFromWebEvent(event);
  np_cocoa_event->data.mouse.deltaX = event.deltaX;
  np_cocoa_event->data.mouse.deltaY = event.deltaY;
  return true;
}

static bool NPCocoaEventFromWebKeyboardEvent(const WebKeyboardEvent& event,
                                             NPCocoaEvent *np_cocoa_event) {
  np_cocoa_event->data.key.keyCode = event.nativeKeyCode;

  np_cocoa_event->data.key.characters = reinterpret_cast<NPNSString*>(
      [NSString stringWithFormat:@"%S", event.text]);
  np_cocoa_event->data.key.charactersIgnoringModifiers =
      reinterpret_cast<NPNSString*>(
          [NSString stringWithFormat:@"%S", event.unmodifiedText]);

  np_cocoa_event->data.key.modifierFlags |= CocoaModifiersFromWebEvent(event);

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

bool WebPluginDelegateImpl::HandleInputEvent(const WebInputEvent& event,
                                             WebCursorInfo* cursor) {
  DCHECK(windowless_) << "events should only be received in windowless mode";
  DCHECK(cursor != NULL);

  // if we do not currently have focus and this is a mouseDown, trigger a
  // notification that we are taking the keyboard focus.  We can't just key
  // off of incoming calls to SetFocus, since WebKit may already think we
  // have it if we were the most recently focused element on our parent tab.
  if (event.type == WebInputEvent::MouseDown && !have_focus_)
    SetFocus();

  if (WebInputEventIsWebMouseEvent(event)) {
    // Make sure we update our plugin location tracking before we send the
    // event to the plugin, so that any coordinate conversion the plugin does
    // will work out.
    const WebMouseEvent* mouse_event =
        static_cast<const WebMouseEvent*>(&event);
    UpdatePluginLocation(*mouse_event);
  }

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
    // If we somehow get an event before we've set up the plugin window, bail.
    if (!cg_context_.context)
      return false;

    if (event.type == WebInputEvent::MouseMove) {
      return true;  // The recurring OnNull will send null events.
    }

    switch (instance()->drawing_model()) {
#ifndef NP_NO_QUICKDRAW
      case NPDrawingModelQuickDraw:
        SetPort(qd_port_.port);
        break;
#endif
      case NPDrawingModelCoreGraphics:
        CGContextSaveGState(cg_context_.context);
        break;
    }
  }
#endif

  // Create the plugin event structure, and send it to the plugin.
  bool ret = false;
  switch (instance()->event_model()) {
#ifndef NP_NO_CARBON
    case NPEventModelCarbon: {
      NPEvent np_event = {0};
      if (!NPEventFromWebInputEvent(event, &np_event)) {
        LOG(WARNING) << "NPEventFromWebInputEvent failed";
        return false;
      }
      // Set this plugin's window as the active one, for global Carbon calls.
      ScopedActivePluginWindow active_window_scope(
          reinterpret_cast<WindowRef>(cg_context_.window));
      ret = instance()->NPP_HandleEvent(&np_event) != 0;
      break;
    }
#endif
    case NPEventModelCocoa: {
      NPCocoaEvent np_cocoa_event;
      if (!NPCocoaEventFromWebInputEvent(event, &np_cocoa_event)) {
        LOG(WARNING) << "NPCocoaEventFromWebInputEvent failed";
        return false;
      }
      NPAPI::ScopedCurrentPluginEvent event_scope(instance(), &np_cocoa_event);
      ret = instance()->NPP_HandleEvent(
          reinterpret_cast<NPEvent*>(&np_cocoa_event)) != 0;
      break;
    }
  }

  if (WebInputEventIsWebMouseEvent(event)) {
    // Plugins are not good about giving accurate information about whether or
    // not they handled events, and other browsers on the Mac generally ignore
    // the return value. We may need to expand this to other input types, but
    // we'll need to be careful about things like Command-keys.
    ret = true;
  }

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon &&
      instance()->drawing_model() == NPDrawingModelCoreGraphics)
    CGContextRestoreGState(cg_context_.context);
#endif

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
  // Avoid a race condition between IO and UI threads during plugin shutdown
  if (!instance_)
    return;
#ifndef NP_NO_CARBON
  if (!webkit_glue::IsPluginRunningInRendererProcess()) {
    switch (instance()->event_model()) {
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

  if (instance()->event_model() == NPEventModelCarbon) {
    // Send an idle event so that the plugin can do background work
    NPEvent np_event = {0};
    np_event.what = nullEvent;
    np_event.when = TickCount();
    np_event.modifiers = GetCurrentKeyModifiers();
    if (!Button())
      np_event.modifiers |= btnState;
    np_event.where.h = last_mouse_x_;
    np_event.where.v = last_mouse_y_;
    // Set this plugin's window as the active one, for global Carbon calls.
    ScopedActivePluginWindow active_window_scope(
        reinterpret_cast<WindowRef>(cg_context_.window));
    instance()->NPP_HandleEvent(&np_event);
  }
#endif

#ifndef NP_NO_QUICKDRAW
  // Quickdraw-based plugins can draw at any time, so tell the renderer to
  // repaint.
  // TODO: only do this if the contents of the offscreen window has changed,
  // so as not to spam the renderer with an unchanging image.
  if (instance()->drawing_model() == NPDrawingModelQuickDraw)
    instance()->webplugin()->Invalidate();
#endif

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        null_event_factory_.NewRunnableMethod(
            &WebPluginDelegateImpl::OnNullEvent),
        kPluginIdleThrottleDelayMs);
  }
#endif
}
