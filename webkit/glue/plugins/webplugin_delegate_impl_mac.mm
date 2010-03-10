// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "webkit/glue/plugins/webplugin_delegate_impl.h"

#include <string>
#include <unistd.h>
#include <set>

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/timer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "webkit/default_plugin/plugin_impl.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/coregraphics_private_symbols_mac.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "webkit/glue/webkit_glue.h"

#ifndef NP_NO_CARBON
#include "webkit/glue/plugins/carbon_plugin_window_tracker_mac.h"
#endif

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

base::LazyInstance<std::set<WebPluginDelegateImpl*> > g_active_delegates(
    base::LINKER_INITIALIZED);

WebPluginDelegateImpl* g_active_delegate;

// Helper to simplify correct usage of g_active_delegate.  Instantiating will
// set the active delegate to |delegate| for the lifetime of the object, then
// NULL when it goes out of scope.
class ScopedActiveDelegate {
public:
  explicit ScopedActiveDelegate(WebPluginDelegateImpl* delegate) {
    g_active_delegate = delegate;
  }
  ~ScopedActiveDelegate() {
    g_active_delegate = NULL;
  }
private:
  DISALLOW_COPY_AND_ASSIGN(ScopedActiveDelegate);
};

#ifndef NP_NO_CARBON
// Timer periods for sending idle events to Carbon plugins. The visible value
// (50Hz) matches both Safari and Firefox. The hidden value (8Hz) matches
// Firefox; according to https://bugzilla.mozilla.org/show_bug.cgi?id=525533
// going lower than that causes issues.
const int kVisibleIdlePeriodMs = 20;     // (50Hz)
const int kHiddenIdlePeriodMs = 125;  // (8Hz)

class CarbonIdleEventSource {
 public:
  // Returns the shared Carbon idle event source.
  static CarbonIdleEventSource* SharedInstance() {
    DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
    static CarbonIdleEventSource* event_source = new CarbonIdleEventSource();
    return event_source;
  }

  // Registers the plugin delegate as interested in receiving idle events at
  // a rate appropriate for the given visibility. A delegate can safely be
  // re-registered any number of times, with the latest registration winning.
  void RegisterDelegate(WebPluginDelegateImpl* delegate, bool visible) {
    if (visible) {
      visible_delegates_->RegisterDelegate(delegate);
      hidden_delegates_->UnregisterDelegate(delegate);
    } else {
      hidden_delegates_->RegisterDelegate(delegate);
      visible_delegates_->UnregisterDelegate(delegate);
    }
  }

  // Removes the plugin delegate from the list of plugins receiving idle events.
  void UnregisterDelegate(WebPluginDelegateImpl* delegate) {
    visible_delegates_->UnregisterDelegate(delegate);
    hidden_delegates_->UnregisterDelegate(delegate);
  }

 private:
  class VisibilityGroup {
   public:
    explicit VisibilityGroup(int timer_period)
        : timer_period_(timer_period), iterator_(delegates_.end()) {}

    // Adds |delegate| to this visibility group.
    void RegisterDelegate(WebPluginDelegateImpl* delegate) {
      if (delegates_.empty()) {
        timer_.Start(base::TimeDelta::FromMilliseconds(timer_period_),
                     this, &VisibilityGroup::SendIdleEvents);
      }
      delegates_.insert(delegate);
    }

    // Removes |delegate| from this visibility group.
    void UnregisterDelegate(WebPluginDelegateImpl* delegate) {
      // If a plugin changes visibility during idle event handling, it
      // may be removed from this set while SendIdleEvents is still iterating;
      // if that happens and it's next on the list, increment the iterator
      // before erasing so that the iteration won't be corrupted.
      if ((iterator_ != delegates_.end()) && (*iterator_ == delegate))
        ++iterator_;
      size_t removed = delegates_.erase(delegate);
      if (removed > 0 && delegates_.empty())
        timer_.Stop();
    }

   private:
    // Fires off idle events for each delegate in the group.
    void SendIdleEvents() {
      for (iterator_ = delegates_.begin(); iterator_ != delegates_.end();) {
        // Pre-increment so that the skip logic in UnregisterDelegates works.
        WebPluginDelegateImpl* delegate = *(iterator_++);
        delegate->FireIdleEvent();
      }
    }

    int timer_period_;
    base::RepeatingTimer<VisibilityGroup> timer_;
    std::set<WebPluginDelegateImpl*> delegates_;
    std::set<WebPluginDelegateImpl*>::iterator iterator_;
  };

  CarbonIdleEventSource()
      : visible_delegates_(new VisibilityGroup(kVisibleIdlePeriodMs)),
        hidden_delegates_(new VisibilityGroup(kHiddenIdlePeriodMs)) {}

  scoped_ptr<VisibilityGroup> visible_delegates_;
  scoped_ptr<VisibilityGroup> hidden_delegates_;

  DISALLOW_COPY_AND_ASSIGN(CarbonIdleEventSource);
};
#endif  // !NP_NO_CARBON

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
      buffer_context_(NULL),
      quirks_(0),
      have_focus_(false),
      focus_notifier_(NULL),
      containing_window_has_focus_(false),
      initial_window_focus_(false),
      container_is_visible_(false),
      have_called_set_window_(false),
      handle_event_depth_(0) {
  memset(&window_, 0, sizeof(window_));
#ifndef NP_NO_CARBON
  memset(&np_cg_context_, 0, sizeof(np_cg_context_));
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

  DestroyInstance();

#ifndef NP_NO_CARBON
  if (np_cg_context_.window) {
    CarbonPluginWindowTracker::SharedInstance()->DestroyDummyWindowForDelegate(
        this, reinterpret_cast<WindowRef>(np_cg_context_.window));
  }
#endif
}

void WebPluginDelegateImpl::PlatformInitialize() {
  // Don't set a NULL window handle on destroy for Mac plugins.  This matches
  // Safari and other Mac browsers (see PluginView::stop() in PluginView.cpp,
  // where code to do so is surrounded by an #ifdef that excludes Mac OS X, or
  // destroyPlugin in WebNetscapePluginView.mm, for examples).
  quirks_ |= PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY;

  // Some plugins don't always unload cleanly, so don't unload them at shutdown.
  if (instance()->mime_type().find("x-silverlight") != std::string::npos ||
      instance()->mime_type().find("audio/x-pn-realaudio") != std::string::npos)
    instance()->plugin_lib()->PreventLibraryUnload();

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
    // Create a stand-in for the browser window so that the plugin will have
    // a non-NULL WindowRef to which it can refer.
    CarbonPluginWindowTracker* window_tracker =
        CarbonPluginWindowTracker::SharedInstance();
    np_cg_context_.window = window_tracker->CreateDummyWindowForDelegate(this);
    np_cg_context_.context = NULL;
    UpdateDummyWindowBounds(gfx::Point(0, 0));
#ifndef NP_NO_QUICKDRAW
    qd_port_.port =
        GetWindowPort(reinterpret_cast<WindowRef>(np_cg_context_.window));
#endif
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
        window_.window = &np_cg_context_;
#endif
      window_.type = NPWindowTypeDrawable;
      break;
    default:
      NOTREACHED();
      break;
  }

  // TODO(stuartmorgan): We need real plugin container visibility information
  // when the plugin is initialized; for now, assume it's visible.
  // None of the calls SetContainerVisibility would make are useful at this
  // point, so we just set the initial state directly.
  container_is_visible_ = true;

#ifndef NP_NO_CARBON
  // If the plugin wants Carbon events, hook up to the source of idle events.
  if (instance()->event_model() == NPEventModelCarbon)
    UpdateIdleEventRate();
#endif
  plugin_->SetWindow(NULL);

  // QuickTime can crash if it gets other calls (e.g., NPP_Write) before it
  // gets a SetWindow call, so call SetWindow (with a 0x0 rect) immediately.
  const WebPluginInfo& plugin_info = instance_->plugin_lib()->plugin_info();
  if (plugin_info.name.find(L"QuickTime") != std::wstring::npos)
    WindowlessSetWindow(true);
}

void WebPluginDelegateImpl::PlatformDestroyInstance() {
#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon)
    CarbonIdleEventSource::SharedInstance()->UnregisterDelegate(this);
#endif
}

void WebPluginDelegateImpl::UpdateGeometryAndContext(
    const gfx::Rect& window_rect, const gfx::Rect& clip_rect,
    CGContextRef context) {
  buffer_context_ = context;
#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
    // Update the structure that is passed to Carbon+CoreGraphics plugins in
    // NPP_SetWindow before calling UpdateGeometry, since that will trigger an
    // NPP_SetWindow call if the geometry changes (which is the only time the
    // context would be different), and some plugins (e.g., Flash) have an
    // internal cache of the context that they only update when NPP_SetWindow
    // is called.
    np_cg_context_.context = context;
  }
#endif
  UpdateGeometry(window_rect, clip_rect);
}

void WebPluginDelegateImpl::Paint(CGContextRef context, const gfx::Rect& rect) {
  WindowlessPaint(context, rect);

#ifndef NP_NO_QUICKDRAW
  // Paint events are our cue to scrape the dummy window into the real context
  // if we are dealing with a QuickDraw plugin.
  // Note that we use buffer_context_ rather than the Paint parameter
  // because the buffer might have changed during the NPP_HandleEvent call
  // in WindowlessPaint.
  if (instance()->drawing_model() == NPDrawingModelQuickDraw) {
    ScrapeDummyWindowIntoContext(buffer_context_);
  }
#endif
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
  bool old_clip_was_empty = clip_rect_.IsEmpty();
  cached_clip_rect_ = clip_rect;
  if (container_is_visible_)  // Remove check when cached_clip_rect_ is removed.
    clip_rect_ = clip_rect;
  bool new_clip_is_empty = clip_rect_.IsEmpty();

  // Only resend to the instance if the geometry has changed (see note in
  // WindowlessSetWindow for why we only care about the clip rect switching
  // empty state).
  if (window_rect == window_rect_ && old_clip_was_empty == new_clip_is_empty)
    return;

#ifndef NP_NO_CARBON
  // If visibility has changed, switch our idle event rate.
  if (instance()->event_model() == NPEventModelCarbon &&
      old_clip_was_empty != new_clip_is_empty) {
    UpdateIdleEventRate();
  }
#endif

  SetPluginRect(window_rect);
  WindowlessSetWindow(true);
}

void WebPluginDelegateImpl::WindowlessPaint(gfx::NativeDrawingContext context,
                                            const gfx::Rect& damage_rect) {
  // If we somehow get a paint before we've set up the plugin buffer, bail.
  if (!buffer_context_)
    return;
  DCHECK(buffer_context_ == context);

  static StatsRate plugin_paint("Plugin.Paint");
  StatsScope<StatsRate> scope(plugin_paint);

  // Plugin invalidates trigger asynchronous paints with the original
  // invalidation rect; the plugin may be resized before the paint is handled,
  // so we need to ensure that the damage rect is still sane.
  const gfx::Rect paint_rect(damage_rect.Intersect(
      gfx::Rect(0, 0, window_rect_.width(), window_rect_.height())));

  ScopedActiveDelegate active_delegate(this);

  CGContextSaveGState(context);

  switch (instance()->event_model()) {
#ifndef NP_NO_CARBON
    case NPEventModelCarbon: {
      NPEvent paint_event = { 0 };
      paint_event.what = updateEvt;
      paint_event.message = reinterpret_cast<uint32>(np_cg_context_.window);
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
      paint_event.data.draw.x = paint_rect.x();
      paint_event.data.draw.y = paint_rect.y();
      paint_event.data.draw.width = paint_rect.width();
      paint_event.data.draw.height = paint_rect.height();
      instance()->NPP_HandleEvent(&paint_event);
      break;
    }
  }

  // The backing buffer can change during the call to NPP_HandleEvent, in which
  // case the old context is (or is about to be) invalid.
  if (context == buffer_context_)
    CGContextRestoreGState(context);
}

void WebPluginDelegateImpl::WindowlessSetWindow(bool force_set_window) {
  if (!instance())
    return;

  window_.x = 0;
  window_.y = 0;
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.clipRect.left = window_.x;
  window_.clipRect.top = window_.y;
  window_.clipRect.right = window_.clipRect.left;
  window_.clipRect.bottom = window_.clipRect.top;
  if (container_is_visible_ && !clip_rect_.IsEmpty()) {
    // We never tell plugins that they are only partially visible; because the
    // drawing target doesn't change size, the positioning of what plugins drew
    // would be wrong, as would any transforms they did on the context.
    window_.clipRect.right += window_.width;
    window_.clipRect.bottom += window_.height;
  }

  NPError err = instance()->NPP_SetWindow(&window_);

  // Send an appropriate window focus event after the first SetWindow.
  if (!have_called_set_window_) {
    have_called_set_window_ = true;
    SetWindowHasFocus(initial_window_focus_);
  }

  DCHECK(err == NPERR_NO_ERROR);
}

WebPluginDelegateImpl* WebPluginDelegateImpl::GetActiveDelegate() {
  return g_active_delegate;
}

std::set<WebPluginDelegateImpl*> WebPluginDelegateImpl::GetActiveDelegates() {
  std::set<WebPluginDelegateImpl*>* delegates = g_active_delegates.Pointer();
  return *delegates;
}

void WebPluginDelegateImpl::FocusChanged(bool has_focus) {
  if (has_focus == have_focus_)
    return;
  have_focus_ = has_focus;

  ScopedActiveDelegate active_delegate(this);

  switch (instance()->event_model()) {
#ifndef NP_NO_CARBON
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
#endif
    case NPEventModelCocoa: {
      NPCocoaEvent focus_event;
      memset(&focus_event, 0, sizeof(focus_event));
      focus_event.type = NPCocoaEventFocusChanged;
      focus_event.data.focus.hasFocus = have_focus_;
      instance()->NPP_HandleEvent(&focus_event);
      break;
    }
  }
}

void WebPluginDelegateImpl::SetFocus() {
  if (focus_notifier_)
    focus_notifier_(this);
  else
    FocusChanged(true);
}

void WebPluginDelegateImpl::SetWindowHasFocus(bool has_focus) {
  // If we get a window focus event before calling SetWindow, just remember the
  // states (WindowlessSetWindow will then send it on the first call).
  if (!have_called_set_window_) {
    initial_window_focus_ = has_focus;
    return;
  }

  if (has_focus == containing_window_has_focus_)
    return;
  containing_window_has_focus_ = has_focus;

  ScopedActiveDelegate active_delegate(this);
  switch (instance()->event_model()) {
#ifndef NP_NO_CARBON
    case NPEventModelCarbon: {
      NPEvent focus_event = { 0 };
      focus_event.what = activateEvt;
      if (has_focus)
        focus_event.modifiers |= activeFlag;
      focus_event.message =
          reinterpret_cast<unsigned long>(np_cg_context_.window);
      focus_event.when = TickCount();
      instance()->NPP_HandleEvent(&focus_event);
      break;
    }
#endif
    case NPEventModelCocoa: {
      NPCocoaEvent focus_event;
      memset(&focus_event, 0, sizeof(focus_event));
      focus_event.type = NPCocoaEventWindowFocusChanged;
      focus_event.data.focus.hasFocus = has_focus;
      instance()->NPP_HandleEvent(&focus_event);
      break;
    }
  }
}

void WebPluginDelegateImpl::SetContainerVisibility(bool is_visible) {
  if (is_visible == container_is_visible_)
    return;
  container_is_visible_ = is_visible;

  // TODO(stuartmorgan): This is a temporary workarond for
  // <http://crbug.com/34266>. When that is fixed, the cached_clip_rect_ code
  // should all be removed.
  if (is_visible) {
    clip_rect_ = cached_clip_rect_;
  } else {
    clip_rect_.set_width(0);
    clip_rect_.set_height(0);
  }

  // TODO(stuartmorgan): We may need to remember whether we had focus, and
  // restore it ourselves when we become visible again. Revisit once SetFocus
  // is actually being called in all the cases it should be, at which point
  // we'll know whether or not that's handled for us by WebKit.
  if (!is_visible)
    FocusChanged(false);

  // If the plugin is changing visibility, let the plugin know. If it's scrolled
  // off screen (i.e., cached_clip_rect_ is empty), then container visibility
  // doesn't change anything.
  if (!cached_clip_rect_.IsEmpty()) {
#ifndef NP_NO_CARBON
    if (instance() && instance()->event_model() == NPEventModelCarbon)
      UpdateIdleEventRate();
#endif
    WindowlessSetWindow(true);
  }
}

void WebPluginDelegateImpl::WindowFrameChanged(gfx::Rect window_frame,
                                               gfx::Rect view_frame) {
  instance()->set_window_frame(window_frame);
  SetContentAreaOrigin(gfx::Point(view_frame.x(), view_frame.y()));
}

void WebPluginDelegateImpl::SetThemeCursor(ThemeCursor cursor) {
  current_windowless_cursor_.InitFromThemeCursor(cursor);
}

void WebPluginDelegateImpl::SetCursor(const Cursor* cursor) {
  current_windowless_cursor_.InitFromCursor(cursor);
}

void WebPluginDelegateImpl::SetNSCursor(NSCursor* cursor) {
  current_windowless_cursor_.InitFromNSCursor(cursor);
}

void WebPluginDelegateImpl::SetPluginRect(const gfx::Rect& rect) {
  window_rect_ = rect;
  PluginScreenLocationChanged();
}

void WebPluginDelegateImpl::SetContentAreaOrigin(const gfx::Point& origin) {
  content_area_origin_ = origin;
  PluginScreenLocationChanged();
}

void WebPluginDelegateImpl::PluginScreenLocationChanged() {
  gfx::Point plugin_origin(content_area_origin_.x() + window_rect_.x(),
                           content_area_origin_.y() + window_rect_.y());
  instance()->set_plugin_origin(plugin_origin);

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
    UpdateDummyWindowBounds(plugin_origin);
  }
#endif
}

#ifndef NP_NO_CARBON
void WebPluginDelegateImpl::UpdateDummyWindowBounds(
    const gfx::Point& plugin_origin) {
  WindowRef window = reinterpret_cast<WindowRef>(np_cg_context_.window);
  Rect current_bounds;
  GetWindowBounds(window, kWindowContentRgn, &current_bounds);

  Rect new_bounds;
  // We never want to resize the window to 0x0, so if the plugin is 0x0 just
  // move the window without resizing it.
  if (window_rect_.width() > 0 && window_rect_.height() > 0) {
    SetRect(&new_bounds, 0, 0, window_rect_.width(), window_rect_.height());
    OffsetRect(&new_bounds, plugin_origin.x(), plugin_origin.y());
  } else {
    new_bounds = current_bounds;
    OffsetRect(&new_bounds, plugin_origin.x() - current_bounds.left,
               plugin_origin.y() - current_bounds.top);
  }

  if (new_bounds.left != current_bounds.left ||
      new_bounds.top != current_bounds.top ||
      new_bounds.right != current_bounds.right ||
      new_bounds.bottom != current_bounds.bottom)
    SetWindowBounds(window, kWindowContentRgn, &new_bounds);
}

#ifndef NP_NO_QUICKDRAW
void WebPluginDelegateImpl::ScrapeDummyWindowIntoContext(CGContextRef context) {
  if (!context)
    return;

  CGRect window_bounds = CGRectMake(0, 0,
                                    window_rect_.width(),
                                    window_rect_.height());
  CGWindowID window_id = HIWindowGetCGWindowID(
      reinterpret_cast<WindowRef>(np_cg_context_.window));
  CGContextSaveGState(context);
  CGContextTranslateCTM(context, 0, window_rect_.height());
  CGContextScaleCTM(context, 1.0, -1.0);
  CGContextCopyWindowCaptureContentsToRect(context, window_bounds,
                                           _CGSDefaultConnection(),
                                           window_id, 0);
  CGContextRestoreGState(context);
}
#endif  // !NP_NO_QUICKDRAW

void WebPluginDelegateImpl::UpdateIdleEventRate() {
  bool plugin_visible = container_is_visible_ && !clip_rect_.IsEmpty();
  CarbonIdleEventSource::SharedInstance()->RegisterDelegate(this,
                                                            plugin_visible);
}
#endif  // !NP_NO_CARBON

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

static bool KeyIsModifier(int native_key_code) {
  switch (native_key_code) {
    case 55:  // Left command
    case 54:  // Right command
    case 58:  // Left option
    case 61:  // Right option
    case 59:  // Left control
    case 62:  // Right control
    case 56:  // Left shift
    case 60:  // Right shift
    case 57:  // Caps lock
      return true;
    default:
      return false;
  }
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
    case WebInputEvent::MouseMove: {
      bool mouse_is_down = (event.modifiers & WebInputEvent::LeftButtonDown) ||
                           (event.modifiers & WebInputEvent::RightButtonDown) ||
                           (event.modifiers & WebInputEvent::MiddleButtonDown);
      np_cocoa_event->type = mouse_is_down ? NPCocoaEventMouseDragged
                                           : NPCocoaEventMouseMoved;
      return true;
    }
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

  np_cocoa_event->data.key.modifierFlags |= CocoaModifiersFromWebEvent(event);

  // Modifier keys have their own event type, and don't get character or
  // repeat data.
  if (KeyIsModifier(event.nativeKeyCode)) {
    np_cocoa_event->type = NPCocoaEventFlagsChanged;
    return true;
  }

  np_cocoa_event->data.key.characters = reinterpret_cast<NPNSString*>(
      [NSString stringWithFormat:@"%S", event.text]);
  np_cocoa_event->data.key.charactersIgnoringModifiers =
      reinterpret_cast<NPNSString*>(
          [NSString stringWithFormat:@"%S", event.unmodifiedText]);

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

bool WebPluginDelegateImpl::PlatformHandleInputEvent(
    const WebInputEvent& event, WebCursorInfo* cursor_info) {
  DCHECK(cursor_info != NULL);

  // If we somehow get an event before we've set up the plugin, bail.
  if (!have_called_set_window_)
    return false;
#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon &&
      !np_cg_context_.context) {
    return false;
  }
#endif

  if (WebInputEventIsWebMouseEvent(event)) {
    // Check our plugin location before we send the event to the plugin, just
    // in case we somehow missed a plugin frame change.
    const WebMouseEvent* mouse_event =
        static_cast<const WebMouseEvent*>(&event);
    gfx::Point content_origin(
        mouse_event->globalX - mouse_event->x - window_rect_.x(),
        mouse_event->globalY - mouse_event->y - window_rect_.y());
    if (content_origin.x() != content_area_origin_.x() ||
        content_origin.y() != content_area_origin_.y()) {
      DLOG(WARNING) << "Stale plugin location: " << content_area_origin_
                    << " instead of " << content_origin;
      SetContentAreaOrigin(content_origin);
    }

    current_windowless_cursor_.GetCursorInfo(cursor_info);
  }

  // if we do not currently have focus and this is a mouseDown, trigger a
  // notification that we are taking the keyboard focus.  We can't just key
  // off of incoming calls to SetFocus, since WebKit may already think we
  // have it if we were the most recently focused element on our parent tab.
  if (event.type == WebInputEvent::MouseDown && !have_focus_) {
    SetFocus();
    // Make sure that the plugin is still there after handling the focus event.
    if (!instance())
      return false;
  }

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
    if (event.type == WebInputEvent::MouseMove) {
      return true;  // The recurring OnNull will send null events.
    }

#ifndef NP_NO_QUICKDRAW
    if (instance()->drawing_model() == NPDrawingModelQuickDraw) {
      SetPort(qd_port_.port);
    }
#endif
  }
#endif

  ScopedActiveDelegate active_delegate(this);

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
      ret = instance()->NPP_HandleEvent(&np_cocoa_event) != 0;
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

  return ret;
}

#ifndef NP_NO_CARBON
void WebPluginDelegateImpl::FireIdleEvent() {
  // Avoid a race condition between IO and UI threads during plugin shutdown
  if (!instance())
    return;

  ScopedActiveDelegate active_delegate(this);

  // Send an idle event so that the plugin can do background work
  NPEvent np_event = {0};
  np_event.what = nullEvent;
  np_event.when = TickCount();
  np_event.modifiers = GetCurrentKeyModifiers();
  if (!Button())
    np_event.modifiers |= btnState;
  HIPoint mouse_location;
  HIGetMousePosition(kHICoordSpaceScreenPixel, NULL, &mouse_location);
  np_event.where.h = mouse_location.x;
  np_event.where.v = mouse_location.y;
  instance()->NPP_HandleEvent(&np_event);

#ifndef NP_NO_QUICKDRAW
  // Quickdraw-based plugins can draw at any time, so tell the renderer to
  // repaint.
  // TODO: only do this if the contents of the offscreen window has changed,
  // so as not to spam the renderer with an unchanging image.
  if (instance() && instance()->drawing_model() == NPDrawingModelQuickDraw)
    instance()->webplugin()->Invalidate();
#endif
}
#endif  // !NP_NO_CARBON
