// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include "webkit/plugins/npapi/webplugin_delegate_impl.h"

#include <string>
#include <unistd.h>
#include <set>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/sys_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/npapi/plugin_instance.h"
#include "webkit/plugins/npapi/plugin_lib.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/plugin_stream_url.h"
#include "webkit/plugins/npapi/plugin_web_event_converter_mac.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/npapi/webplugin_accelerated_surface_mac.h"

#ifndef NP_NO_CARBON
#include "webkit/plugins/npapi/carbon_plugin_window_tracker_mac.h"
#endif

#ifndef NP_NO_QUICKDRAW
#include "webkit/plugins/npapi/quickdraw_drawing_manager_mac.h"
#endif

using WebKit::WebCursorInfo;
using WebKit::WebKeyboardEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

// Important implementation notes: The Mac definition of NPAPI, particularly
// the distinction between windowed and windowless modes, differs from the
// Windows and Linux definitions.  Most of those differences are
// accomodated by the WebPluginDelegate class.

namespace webkit {
namespace npapi {

namespace {

const int kCoreAnimationRedrawPeriodMs = 10;  // 100 Hz

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

// Helper to build and maintain a model of a drag entering the plugin but not
// starting there. See explanation in PlatformHandleInputEvent.
class ExternalDragTracker {
 public:
  ExternalDragTracker() : pressed_buttons_(0) {}

  // Returns true if an external drag is in progress.
  bool IsDragInProgress() { return pressed_buttons_ != 0; };

  // Returns true if the given event appears to be related to an external drag.
  bool EventIsRelatedToDrag(const WebInputEvent& event);

  // Updates the tracking of whether an external drag is in progress--and if
  // so what buttons it involves--based on the given event.
  void UpdateDragStateFromEvent(const WebInputEvent& event);

 private:
  // Returns the mask for just the button state in a WebInputEvent's modifiers.
  static int WebEventButtonModifierMask();

  // The WebInputEvent modifier flags for any buttons that were down when an
  // external drag entered the plugin, and which and are still down now.
  int pressed_buttons_;

  DISALLOW_COPY_AND_ASSIGN(ExternalDragTracker);
};

void ExternalDragTracker::UpdateDragStateFromEvent(const WebInputEvent& event) {
  switch (event.type) {
    case WebInputEvent::MouseEnter:
      pressed_buttons_ = event.modifiers & WebEventButtonModifierMask();
      break;
    case WebInputEvent::MouseUp: {
      const WebMouseEvent* mouse_event =
          static_cast<const WebMouseEvent*>(&event);
      if (mouse_event->button == WebMouseEvent::ButtonLeft)
        pressed_buttons_ &= ~WebInputEvent::LeftButtonDown;
      if (mouse_event->button == WebMouseEvent::ButtonMiddle)
        pressed_buttons_ &= ~WebInputEvent::MiddleButtonDown;
      if (mouse_event->button == WebMouseEvent::ButtonRight)
        pressed_buttons_ &= ~WebInputEvent::RightButtonDown;
      break;
    }
    default:
      break;
  }
}

bool ExternalDragTracker::EventIsRelatedToDrag(const WebInputEvent& event) {
  const WebMouseEvent* mouse_event = static_cast<const WebMouseEvent*>(&event);
  switch (event.type) {
    case WebInputEvent::MouseUp:
      // We only care about release of buttons that were part of the drag.
      return ((mouse_event->button == WebMouseEvent::ButtonLeft &&
               (pressed_buttons_ & WebInputEvent::LeftButtonDown)) ||
              (mouse_event->button == WebMouseEvent::ButtonMiddle &&
               (pressed_buttons_ & WebInputEvent::MiddleButtonDown)) ||
              (mouse_event->button == WebMouseEvent::ButtonRight &&
               (pressed_buttons_ & WebInputEvent::RightButtonDown)));
    case WebInputEvent::MouseEnter:
      return (event.modifiers & WebEventButtonModifierMask()) != 0;
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseMove: {
      int event_buttons = (event.modifiers & WebEventButtonModifierMask());
      return (pressed_buttons_ &&
              pressed_buttons_ == event_buttons);
    }
    default:
      return false;
  }
  return false;
}

int ExternalDragTracker::WebEventButtonModifierMask() {
  return WebInputEvent::LeftButtonDown |
         WebInputEvent::RightButtonDown |
         WebInputEvent::MiddleButtonDown;
}

#pragma mark -
#pragma mark Core WebPluginDelegate implementation

WebPluginDelegateImpl::WebPluginDelegateImpl(
    gfx::PluginWindowHandle containing_view,
    PluginInstance *instance)
    : windowed_handle_(NULL),
      // all Mac plugins are "windowless" in the Windows/X11 sense
      windowless_(true),
      plugin_(NULL),
      instance_(instance),
      parent_(containing_view),
      quirks_(0),
      buffer_context_(NULL),
      layer_(nil),
      surface_(NULL),
      renderer_(nil),
      containing_window_has_focus_(false),
      initial_window_focus_(false),
      container_is_visible_(false),
      have_called_set_window_(false),
      ime_enabled_(false),
      keyup_ignore_count_(0),
      external_drag_tracker_(new ExternalDragTracker()),
      handle_event_depth_(0),
      first_set_window_call_(true),
      plugin_has_focus_(false),
      has_webkit_focus_(false),
      containing_view_has_focus_(true),
      creation_succeeded_(false) {
  memset(&window_, 0, sizeof(window_));
#ifndef NP_NO_CARBON
  memset(&np_cg_context_, 0, sizeof(np_cg_context_));
#endif
#ifndef NP_NO_QUICKDRAW
  memset(&qd_port_, 0, sizeof(qd_port_));
#endif
  instance->set_windowless(true);
}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
  DestroyInstance();

#ifndef NP_NO_CARBON
  if (np_cg_context_.window) {
    CarbonPluginWindowTracker::SharedInstance()->DestroyDummyWindowForDelegate(
        this, reinterpret_cast<WindowRef>(np_cg_context_.window));
  }
#endif
}

bool WebPluginDelegateImpl::PlatformInitialize() {
  // Don't set a NULL window handle on destroy for Mac plugins.  This matches
  // Safari and other Mac browsers (see PluginView::stop() in PluginView.cpp,
  // where code to do so is surrounded by an #ifdef that excludes Mac OS X, or
  // destroyPlugin in WebNetscapePluginView.mm, for examples).
  quirks_ |= PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY;

  // Mac plugins don't expect to be unloaded, and they don't always do so
  // cleanly, so don't unload them at shutdown.
  instance()->plugin_lib()->PreventLibraryUnload();

#ifndef NP_NO_QUICKDRAW
  if (instance()->drawing_model() == NPDrawingModelQuickDraw) {
    // For some QuickDraw plugins, we can sometimes get away with giving them
    // a port pointing to a pixel buffer instead of a our actual dummy window.
    // This gives us much better frame rates, because the window scraping we
    // normally use is very slow.
    // This breaks down if the plugin does anything complicated with the port
    // (as QuickTime seems to during event handling, and sometimes when painting
    // its controls), so we switch on the fly as necessary. (It might be
    // possible to interpose sufficiently that we wouldn't have to switch back
    // and forth, but the current approach gets us most of the benefit.)
    // We can't do this at all with plugins that bypass the port entirely and
    // attaches their own surface to the window.
    // TODO(stuartmorgan): Test other QuickDraw plugins that we support and
    // see if any others can use the fast path.
    const WebPluginInfo& plugin_info = instance_->plugin_lib()->plugin_info();
    if (plugin_info.name.find(ASCIIToUTF16("QuickTime")) != string16::npos)
      quirks_ |= PLUGIN_QUIRK_ALLOW_FASTER_QUICKDRAW_PATH;
  }
#endif

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
    // Create a stand-in for the browser window so that the plugin will have
    // a non-NULL WindowRef to which it can refer.
    CarbonPluginWindowTracker* window_tracker =
        CarbonPluginWindowTracker::SharedInstance();
    np_cg_context_.window = window_tracker->CreateDummyWindowForDelegate(this);
    np_cg_context_.context = NULL;
    UpdateDummyWindowBounds(gfx::Point(0, 0));
  }
#endif

  NPDrawingModel drawing_model = instance()->drawing_model();
  switch (drawing_model) {
#ifndef NP_NO_QUICKDRAW
    case NPDrawingModelQuickDraw:
      if (instance()->event_model() != NPEventModelCarbon)
        return false;
      qd_manager_.reset(new QuickDrawDrawingManager());
      qd_manager_->SetPluginWindow(
          reinterpret_cast<WindowRef>(np_cg_context_.window));
      qd_port_.port = qd_manager_->port();
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
    case NPDrawingModelCoreAnimation:
    case NPDrawingModelInvalidatingCoreAnimation: {
      if (instance()->event_model() != NPEventModelCocoa)
        return false;
      window_.type = NPWindowTypeDrawable;
      // Ask the plug-in for the CALayer it created for rendering content.
      // Create a surface to host it, and request a "window" handle to identify
      // the surface.
      CALayer* layer = nil;
      NPError err = instance()->NPP_GetValue(NPPVpluginCoreAnimationLayer,
                                             reinterpret_cast<void*>(&layer));
      if (!err) {
        if (drawing_model == NPDrawingModelCoreAnimation) {
          // Create the timer; it will be started when we get a window handle.
          redraw_timer_.reset(new base::RepeatingTimer<WebPluginDelegateImpl>);
        }
        layer_ = layer;
        surface_ = plugin_->GetAcceleratedSurface();

        // If surface initialization fails for some reason, just continue
        // without any drawing; returning false would be a more confusing user
        // experience (since it triggers a missing plugin placeholder).
        if (surface_ && surface_->context()) {
          renderer_ = [[CARenderer rendererWithCGLContext:surface_->context()
                                                  options:NULL] retain];
          [renderer_ setLayer:layer_];
        }
        plugin_->BindFakePluginWindowHandle(false);
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  // Let the WebPlugin know that we are windowless (unless this is a
  // Core Animation plugin, in which case BindFakePluginWindowHandle will take
  // care of setting up the appropriate window handle).
  if (!layer_)
    plugin_->SetWindow(NULL);

#ifndef NP_NO_CARBON
  // If the plugin wants Carbon events, hook up to the source of idle events.
  if (instance()->event_model() == NPEventModelCarbon)
    UpdateIdleEventRate();
#endif

  // QuickTime (in QD mode only) can crash if it gets other calls (e.g.,
  // NPP_Write) before it gets a SetWindow call, so call SetWindow (with a 0x0
  // rect) immediately.
#ifndef NP_NO_QUICKDRAW
  if (instance()->drawing_model() == NPDrawingModelQuickDraw) {
    const WebPluginInfo& plugin_info = instance_->plugin_lib()->plugin_info();
    if (plugin_info.name.find(ASCIIToUTF16("QuickTime")) != string16::npos)
      WindowlessSetWindow();
  }
#endif

  return true;
}

void WebPluginDelegateImpl::PlatformDestroyInstance() {
#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon)
    CarbonIdleEventSource::SharedInstance()->UnregisterDelegate(this);
#endif
  if (redraw_timer_.get())
    redraw_timer_->Stop();
  [renderer_ release];
  renderer_ = nil;
  layer_ = nil;
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
#ifndef NP_NO_QUICKDRAW
  if (instance()->drawing_model() == NPDrawingModelQuickDraw)
    qd_manager_->SetTargetContext(context, window_rect.size());
#endif
  UpdateGeometry(window_rect, clip_rect);
}

void WebPluginDelegateImpl::Paint(CGContextRef context, const gfx::Rect& rect) {
  WindowlessPaint(context, rect);

#ifndef NP_NO_QUICKDRAW
  // Paint events are our cue to dump the current plugin bits into the buffer
  // context if we are dealing with a QuickDraw plugin.
  if (instance()->drawing_model() == NPDrawingModelQuickDraw) {
    qd_manager_->UpdateContext();
  }
#endif
}

void WebPluginDelegateImpl::Print(CGContextRef context) {
  NOTIMPLEMENTED();
}

bool WebPluginDelegateImpl::PlatformHandleInputEvent(
    const WebInputEvent& event, WebCursorInfo* cursor_info) {
  DCHECK(cursor_info != NULL);

  // If we get an event before we've set up the plugin, bail.
  if (!have_called_set_window_)
    return false;
#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon &&
      !np_cg_context_.context) {
    return false;
  }
#endif

  if (WebInputEvent::isMouseEventType(event.type) ||
      event.type == WebInputEvent::MouseWheel) {
    // Check our plugin location before we send the event to the plugin, just
    // in case we somehow missed a plugin frame change.
    const WebMouseEvent* mouse_event =
        static_cast<const WebMouseEvent*>(&event);
    gfx::Point content_origin(
        mouse_event->globalX - mouse_event->x - window_rect_.x(),
        mouse_event->globalY - mouse_event->y - window_rect_.y());
    if (content_origin.x() != content_area_origin_.x() ||
        content_origin.y() != content_area_origin_.y()) {
      DLOG(WARNING) << "Stale plugin content area location: "
                    << content_area_origin_ << " instead of "
                    << content_origin;
      SetContentAreaOrigin(content_origin);
    }

    current_windowless_cursor_.GetCursorInfo(cursor_info);
  }

  // Per the Cocoa Plugin IME spec, plugins shoudn't receive keydown or keyup
  // events while composition is in progress. Treat them as handled, however,
  // since IME is consuming them on behalf of the plugin.
  if ((event.type == WebInputEvent::KeyDown && ime_enabled_) ||
      (event.type == WebInputEvent::KeyUp && keyup_ignore_count_)) {
    // Composition ends on a keydown, so ime_enabled_ will be false at keyup;
    // because the keydown wasn't sent to the plugin, the keyup shouldn't be
    // either (per the spec).
    if (event.type == WebInputEvent::KeyDown)
      ++keyup_ignore_count_;
    else
      --keyup_ignore_count_;
    return true;
  }

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
#ifndef NP_NO_QUICKDRAW
    if (instance()->drawing_model() == NPDrawingModelQuickDraw) {
      if (quirks_ & PLUGIN_QUIRK_ALLOW_FASTER_QUICKDRAW_PATH) {
        // Mouse event handling doesn't work correctly in the fast path mode,
        // so any time we get a mouse event turn the fast path off, but set a
        // time to switch it on again (we don't rely just on MouseLeave because
        // we don't want poor performance in the case of clicking the play
        // button and then leaving the mouse there).
        // This isn't perfect (specifically, click-and-hold doesn't seem to work
        // if the fast path is on), but the slight regression is worthwhile
        // for the improved framerates.
        if (WebInputEvent::isMouseEventType(event.type)) {
          if (event.type == WebInputEvent::MouseLeave) {
            SetQuickDrawFastPathEnabled(true);
          } else {
            SetQuickDrawFastPathEnabled(false);
          }
          // Make sure the plugin wasn't destroyed during the switch.
          if (!instance())
            return false;
        }
      }

      qd_manager_->MakePortCurrent();
    }
#endif

    if (event.type == WebInputEvent::MouseMove) {
      return true;  // The recurring FireIdleEvent will send null events.
    }
  }
#endif

  ScopedActiveDelegate active_delegate(this);

  // Create the plugin event structure.
  NPEventModel event_model = instance()->event_model();
  scoped_ptr<PluginWebEventConverter> event_converter(
      PluginWebEventConverterFactory::CreateConverterForModel(event_model));
  if (!(event_converter.get() && event_converter->InitWithEvent(event))) {
    // Silently consume any keyboard event types that we don't handle, so that
    // they don't fall through to the page.
    if (WebInputEvent::isKeyboardEventType(event.type))
      return true;
    return false;
  }
  void* plugin_event = event_converter->plugin_event();

  if (instance()->event_model() == NPEventModelCocoa) {
    // We recieve events related to drags starting outside the plugin, but the
    // NPAPI Cocoa event model spec says plugins shouldn't receive them, so
    // filter them out.
    // If we add a page capture mode at the WebKit layer (like the plugin
    // capture mode that handles drags starting inside) this can be removed.
    bool drag_related = external_drag_tracker_->EventIsRelatedToDrag(event);
    external_drag_tracker_->UpdateDragStateFromEvent(event);
    if (drag_related) {
      if (event.type == WebInputEvent::MouseUp &&
          !external_drag_tracker_->IsDragInProgress()) {
        // When an external drag ends, we need to synthesize a MouseEntered.
        NPCocoaEvent enter_event = *(static_cast<NPCocoaEvent*>(plugin_event));
        enter_event.type = NPCocoaEventMouseEntered;
        ScopedCurrentPluginEvent event_scope(instance(), &enter_event);
        instance()->NPP_HandleEvent(&enter_event);
      }
      return false;
    }
  }

  // Send the plugin the event.
  scoped_ptr<ScopedCurrentPluginEvent> event_scope(NULL);
  if (instance()->event_model() == NPEventModelCocoa) {
    event_scope.reset(new ScopedCurrentPluginEvent(
        instance(), static_cast<NPCocoaEvent*>(plugin_event)));
  }
  int16_t handle_response = instance()->NPP_HandleEvent(plugin_event);
  bool handled = handle_response != kNPEventNotHandled;

  // Start IME if requested by the plugin.
  if (handled && handle_response == kNPEventStartIME &&
      event.type == WebInputEvent::KeyDown) {
    StartIme();
    ++keyup_ignore_count_;
  }

  // Plugins don't give accurate information about whether or not they handled
  // events, so browsers on the Mac ignore the return value.
  // Scroll events are the exception, since the Cocoa spec defines a meaning
  // for the return value.
  if (WebInputEvent::isMouseEventType(event.type)) {
    handled = true;
  } else if (WebInputEvent::isKeyboardEventType(event.type)) {
    // For Command-key events, trust the return value since eating all menu
    // shortcuts is not ideal.
    // TODO(stuartmorgan): Implement the advanced key handling spec, and trust
    // trust the key event return value from plugins that implement it.
    if (!(event.modifiers & WebInputEvent::MetaKey))
      handled = true;
  }

  return handled;
}

void WebPluginDelegateImpl::InstallMissingPlugin() {
  NOTIMPLEMENTED();
}

#pragma mark -

void WebPluginDelegateImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  gfx::Rect old_clip_rect = clip_rect_;
  cached_clip_rect_ = clip_rect;
  if (container_is_visible_)  // Remove check when cached_clip_rect_ is removed.
    clip_rect_ = clip_rect;
  bool clip_rect_changed = (clip_rect_ != old_clip_rect);
  bool window_size_changed = (window_rect.size() != window_rect_.size());

  bool force_set_window = false;
#ifndef NP_NO_QUICKDRAW
  // In a QuickDraw plugin, a geometry update might have caused a port change;
  // if so, we need to call SetWindow even if nothing else changed.
  if (qd_manager_.get() && (qd_port_.port != qd_manager_->port())) {
    qd_port_.port = qd_manager_->port();
    force_set_window = true;
  }
#endif

  if (window_rect == window_rect_ && !clip_rect_changed && !force_set_window)
    return;

  if (old_clip_rect.IsEmpty() != clip_rect_.IsEmpty()) {
    PluginVisibilityChanged();
  }

  SetPluginRect(window_rect);

#ifndef NP_NO_QUICKDRAW
  if (window_size_changed && qd_manager_.get() &&
      qd_manager_->IsFastPathEnabled()) {
    // If the window size has changed, we need to turn off the fast path so that
    // the full redraw goes to the window and we get a correct baseline paint.
    SetQuickDrawFastPathEnabled(false);
    return;  // SetQuickDrawFastPathEnabled will call SetWindow for us.
  }
#endif

  if (window_size_changed || clip_rect_changed || force_set_window)
    WindowlessSetWindow();
}

void WebPluginDelegateImpl::WindowlessPaint(gfx::NativeDrawingContext context,
                                            const gfx::Rect& damage_rect) {
  // If we get a paint event before we are completely set up (e.g., a nested
  // call while the plugin is still in NPP_SetWindow), bail.
  if (!have_called_set_window_ || !buffer_context_)
    return;
  DCHECK(buffer_context_ == context);

  static base::StatsRate plugin_paint("Plugin.Paint");
  base::StatsScope<base::StatsRate> scope(plugin_paint);

  // Plugin invalidates trigger asynchronous paints with the original
  // invalidation rect; the plugin may be resized before the paint is handled,
  // so we need to ensure that the damage rect is still sane.
  const gfx::Rect paint_rect(damage_rect.Intersect(
      gfx::Rect(0, 0, window_rect_.width(), window_rect_.height())));

  ScopedActiveDelegate active_delegate(this);

#ifndef NP_NO_QUICKDRAW
  if (instance()->drawing_model() == NPDrawingModelQuickDraw)
    qd_manager_->MakePortCurrent();
#endif

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

void WebPluginDelegateImpl::WindowlessSetWindow() {
  if (!instance())
    return;

  window_.x = 0;
  window_.y = 0;
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();

  NPError err = instance()->NPP_SetWindow(&window_);

  // Send an appropriate window focus event after the first SetWindow.
  if (!have_called_set_window_) {
    have_called_set_window_ = true;
    SetWindowHasFocus(initial_window_focus_);
  }

#ifndef NP_NO_QUICKDRAW
  if ((quirks_ & PLUGIN_QUIRK_ALLOW_FASTER_QUICKDRAW_PATH) &&
      !qd_manager_->IsFastPathEnabled() && !clip_rect_.IsEmpty()) {
    // Give the plugin a few seconds to stabilize so we get a good initial paint
    // to use as a baseline, then switch to the fast path.
    fast_path_enable_tick_ = base::TimeTicks::Now() +
        base::TimeDelta::FromSeconds(3);
  }
#endif

  DCHECK(err == NPERR_NO_ERROR);
}

#pragma mark -

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

#pragma mark -
#pragma mark Mac Extensions

void WebPluginDelegateImpl::PluginDidInvalidate() {
  if (instance()->drawing_model() == NPDrawingModelInvalidatingCoreAnimation)
    DrawLayerInSurface();
}

WebPluginDelegateImpl* WebPluginDelegateImpl::GetActiveDelegate() {
  return g_active_delegate;
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

#ifndef NP_NO_QUICKDRAW
  // Make sure controls repaint with the correct look.
  if (quirks_ & PLUGIN_QUIRK_ALLOW_FASTER_QUICKDRAW_PATH)
    SetQuickDrawFastPathEnabled(false);
#endif

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

bool WebPluginDelegateImpl::PlatformSetPluginHasFocus(bool focused) {
  if (!have_called_set_window_)
    return false;

  plugin_->FocusChanged(focused);

  ScopedActiveDelegate active_delegate(this);

  switch (instance()->event_model()) {
#ifndef NP_NO_CARBON
    case NPEventModelCarbon: {
      NPEvent focus_event = { 0 };
      if (focused)
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
      focus_event.data.focus.hasFocus = focused;
      instance()->NPP_HandleEvent(&focus_event);
      break;
    }
  }
  return true;
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

  // If the plugin is changing visibility, let the plugin know. If it's scrolled
  // off screen (i.e., cached_clip_rect_ is empty), then container visibility
  // doesn't change anything.
  if (!cached_clip_rect_.IsEmpty()) {
    PluginVisibilityChanged();
    WindowlessSetWindow();
  }

  // When the plugin become visible, send an empty invalidate. If there were any
  // pending invalidations this will trigger a paint event for the damaged
  // region, and if not it's a no-op. This is necessary since higher levels
  // that would normally do this weren't responsible for the clip_rect_ change).
  if (!clip_rect_.IsEmpty())
    instance()->webplugin()->InvalidateRect(gfx::Rect());
}

void WebPluginDelegateImpl::WindowFrameChanged(const gfx::Rect& window_frame,
                                               const gfx::Rect& view_frame) {
  instance()->set_window_frame(window_frame);
  SetContentAreaOrigin(gfx::Point(view_frame.x(), view_frame.y()));
}

void WebPluginDelegateImpl::ImeCompositionCompleted(const string16& text) {
  if (instance()->event_model() != NPEventModelCocoa) {
    DLOG(ERROR) << "IME notification receieved in Carbon event model";
    return;
  }

  ime_enabled_ = false;

  // If |text| is empty this was just called to tell us composition was
  // cancelled externally (e.g., the user pressed esc).
  if (!text.empty()) {
    NPCocoaEvent text_event;
    memset(&text_event, 0, sizeof(NPCocoaEvent));
    text_event.type = NPCocoaEventTextInput;
    text_event.data.text.text =
        reinterpret_cast<NPNSString*>(base::SysUTF16ToNSString(text));
    instance()->NPP_HandleEvent(&text_event);
  }
}

#ifndef NP_NO_CARBON
void WebPluginDelegateImpl::SetThemeCursor(ThemeCursor cursor) {
  current_windowless_cursor_.InitFromThemeCursor(cursor);
}

void WebPluginDelegateImpl::SetCarbonCursor(const Cursor* cursor) {
  current_windowless_cursor_.InitFromCursor(cursor);
}
#endif

void WebPluginDelegateImpl::SetNSCursor(NSCursor* cursor) {
  current_windowless_cursor_.InitFromNSCursor(cursor);
}

#pragma mark -
#pragma mark Internal Tracking

void WebPluginDelegateImpl::SetPluginRect(const gfx::Rect& rect) {
  bool plugin_size_changed = rect.width() != window_rect_.width() ||
                             rect.height() != window_rect_.height();
  window_rect_ = rect;
  PluginScreenLocationChanged();
  if (plugin_size_changed)
    UpdateAcceleratedSurface();
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

void WebPluginDelegateImpl::PluginVisibilityChanged() {
#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon)
    UpdateIdleEventRate();
#endif
  if (instance()->drawing_model() == NPDrawingModelCoreAnimation) {
    bool plugin_visible = container_is_visible_ && !clip_rect_.IsEmpty();
    if (plugin_visible && !redraw_timer_->IsRunning() && windowed_handle()) {
      redraw_timer_->Start(
          base::TimeDelta::FromMilliseconds(kCoreAnimationRedrawPeriodMs),
          this, &WebPluginDelegateImpl::DrawLayerInSurface);
    } else if (!plugin_visible) {
      redraw_timer_->Stop();
    }
  }
}

void WebPluginDelegateImpl::StartIme() {
  if (instance()->event_model() != NPEventModelCocoa ||
      !IsImeSupported()) {
    return;
  }
  if (ime_enabled_)
    return;
  ime_enabled_ = true;
  plugin_->StartIme();
}

bool WebPluginDelegateImpl::IsImeSupported() {
  // Currently the plugin IME implementation only works on 10.6.
  static BOOL sImeSupported = NO;
  static BOOL sHaveCheckedSupport = NO;
  if (!sHaveCheckedSupport) {
    int32 major, minor, bugfix;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    sImeSupported = major > 10 || (major == 10 && minor > 5);
    sHaveCheckedSupport = YES;
  }
  return sImeSupported;
}

#pragma mark -
#pragma mark Core Animation Support

void WebPluginDelegateImpl::DrawLayerInSurface() {
  // If we haven't plumbed up the surface yet, don't try to draw.
  if (!windowed_handle() || !renderer_)
    return;

  [renderer_ beginFrameAtTime:CACurrentMediaTime() timeStamp:NULL];
  if (CGRectIsEmpty([renderer_ updateBounds])) {
    // If nothing has changed, we are done.
    [renderer_ endFrame];
    return;
  }

  surface_->StartDrawing();

  CGRect layerRect = [layer_ bounds];
  [renderer_ addUpdateRect:layerRect];
  [renderer_ render];
  [renderer_ endFrame];

  surface_->EndDrawing();
}

// Update the size of the surface to match the current size of the plug-in.
void WebPluginDelegateImpl::UpdateAcceleratedSurface() {
  // Will only have a window handle when using a Core Animation drawing model.
  if (!windowed_handle() || !layer_)
    return;

  [CATransaction begin];
  [CATransaction setValue:[NSNumber numberWithInt:0]
                   forKey:kCATransactionAnimationDuration];
  [layer_ setFrame:CGRectMake(0, 0,
                              window_rect_.width(), window_rect_.height())];
  [CATransaction commit];

  [renderer_ setBounds:[layer_ bounds]];
  surface_->SetSize(window_rect_.size());
}

void WebPluginDelegateImpl::set_windowed_handle(
    gfx::PluginWindowHandle handle) {
  windowed_handle_ = handle;
  surface_->SetWindowHandle(handle);
  UpdateAcceleratedSurface();
  // Kick off the drawing timer, if necessary.
  PluginVisibilityChanged();
}

#pragma mark -
#pragma mark Carbon Event support

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

void WebPluginDelegateImpl::UpdateIdleEventRate() {
  bool plugin_visible = container_is_visible_ && !clip_rect_.IsEmpty();
  CarbonIdleEventSource::SharedInstance()->RegisterDelegate(this,
                                                            plugin_visible);
}

void WebPluginDelegateImpl::FireIdleEvent() {
  // Avoid a race condition between IO and UI threads during plugin shutdown
  if (!instance())
    return;
  // Don't send idle events until we've called SetWindow.
  if (!have_called_set_window_)
    return;

#ifndef NP_NO_QUICKDRAW
  // Check whether it's time to turn the QuickDraw fast path back on.
  if (!fast_path_enable_tick_.is_null() &&
      (base::TimeTicks::Now() > fast_path_enable_tick_)) {
    SetQuickDrawFastPathEnabled(true);
    fast_path_enable_tick_ = base::TimeTicks();
  }
#endif

  ScopedActiveDelegate active_delegate(this);

#ifndef NP_NO_QUICKDRAW
  if (instance()->drawing_model() == NPDrawingModelQuickDraw)
    qd_manager_->MakePortCurrent();
#endif

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
  if (instance() && instance()->drawing_model() == NPDrawingModelQuickDraw)
    instance()->webplugin()->Invalidate();
#endif
}
#endif  // !NP_NO_CARBON

#pragma mark -
#pragma mark QuickDraw Support

#ifndef NP_NO_QUICKDRAW
void WebPluginDelegateImpl::SetQuickDrawFastPathEnabled(bool enabled) {
  if (!enabled) {
    // Wait a couple of seconds, then turn the fast path back on. If we're
    // turning it off for event handling, that ensures that the common case of
    // move-mouse-then-click works (as well as making it likely that a second
    // click attempt will work if the first one fails). If we're turning it
    // off to force a new baseline image, this leaves plenty of time for the
    // plugin to draw.
    fast_path_enable_tick_ = base::TimeTicks::Now() +
        base::TimeDelta::FromSeconds(2);
  }

  if (enabled == qd_manager_->IsFastPathEnabled())
    return;
  if (enabled && clip_rect_.IsEmpty()) {
    // Don't switch to the fast path while the plugin is completely clipped;
    // we can only switch when the window has an up-to-date image for us to
    // scrape. We'll automatically switch after we become visible again.
    return;
  }

  qd_manager_->SetFastPathEnabled(enabled);
  qd_port_.port = qd_manager_->port();
  WindowlessSetWindow();
  // Send a paint event so that the new buffer gets updated immediately.
  WindowlessPaint(buffer_context_, clip_rect_);
}
#endif  // !NP_NO_QUICKDRAW

}  // namespace npapi
}  // namespace webkit
