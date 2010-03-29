// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

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
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "webkit/default_plugin/plugin_impl.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/coregraphics_private_symbols_mac.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "webkit/glue/plugins/plugin_web_event_converter_mac.h"
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

const int kCoreAnimationRedrawPeriodMs = 20;  // 50fps

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

#pragma mark -

WebPluginDelegateImpl::WebPluginDelegateImpl(
    gfx::PluginWindowHandle containing_view,
    NPAPI::PluginInstance *instance)
    : windowed_handle_(NULL),
      windowless_needs_set_window_(true),
      // all Mac plugins are "windowless" in the Windows/X11 sense
      windowless_(true),
      plugin_(NULL),
      instance_(instance),
      parent_(containing_view),
      buffer_context_(NULL),
#ifndef NP_NO_QUICKDRAW
      qd_buffer_world_(NULL),
      qd_plugin_world_(NULL),
      qd_fast_path_enabled_(false),
#endif
      layer_(nil),
      surface_(NULL),
      renderer_(nil),
      quirks_(0),
      have_focus_(false),
      external_drag_buttons_(0),
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
#ifndef NP_NO_QUICKDRAW
  if (quirks_ & PLUGIN_QUIRK_ALLOW_FASTER_QUICKDRAW_PATH)
    UpdateGWorlds(NULL);
#endif
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
    if (plugin_info.name.find(L"QuickTime") != std::wstring::npos)
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
    case NPDrawingModelCoreAnimation: {  // Assumes Cocoa event model.
      window_.type = NPWindowTypeDrawable;
      // Ask the plug-in for the CALayer it created for rendering content. Have
      // the renderer tell the browser to create a "windowed plugin" to host
      // the IOSurface.
      CALayer* layer = nil;
      NPError err = instance()->NPP_GetValue(NPPVpluginCoreAnimationLayer,
                                             reinterpret_cast<void*>(&layer));
      if (!err) {
        layer_ = layer;
        plugin_->BindFakePluginWindowHandle();
        surface_ = new AcceleratedSurface;
        surface_->Initialize();
        renderer_ = [[CARenderer rendererWithCGLContext:surface_->context()
                                                options:NULL] retain];
        [renderer_ setLayer:layer_];
        UpdateAcceleratedSurface();
        redraw_timer_.reset(new base::RepeatingTimer<WebPluginDelegateImpl>);
        // This will start the timer, but only if we are visible.
        PluginVisibilityChanged();
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  // TODO(stuartmorgan): We need real plugin container visibility information
  // when the plugin is initialized; for now, assume it's visible.
  // None of the calls SetContainerVisibility would make are useful at this
  // point, so we just set the initial state directly.
  container_is_visible_ = true;

  // Let the WebPlugin know that we are windowless (unless this is a
  // Core Animation plugin, in which case BindFakePluginWindowHandle will take
  // care of setting up the appropriate window handle).
  if (instance()->drawing_model() != NPDrawingModelCoreAnimation)
    plugin_->SetWindow(NULL);

#ifndef NP_NO_CARBON
  // If the plugin wants Carbon events, hook up to the source of idle events.
  if (instance()->event_model() == NPEventModelCarbon)
    UpdateIdleEventRate();
#endif

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
  if (redraw_timer_.get())
    redraw_timer_->Stop();
  [renderer_ release];
  renderer_ = nil;
  layer_ = nil;
  if (surface_) {
    surface_->Destroy();
    delete surface_;
    surface_ = NULL;
  }
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
  // (slow path) or copy the offscreen GWorld bits into the context (fast path)
  // if we are dealing with a QuickDraw plugin.
  // Note that we use buffer_context_ rather than |context| because the buffer
  // might have changed during the NPP_HandleEvent call in WindowlessPaint.
  if (instance()->drawing_model() == NPDrawingModelQuickDraw) {
    if (qd_fast_path_enabled_)
      CopyGWorldBits(qd_plugin_world_, qd_buffer_world_);
    else
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

  bool window_rect_changed = (window_rect != window_rect_);

  // Only resend to the instance if the geometry has changed (see note in
  // WindowlessSetWindow for why we only care about the clip rect switching
  // empty state).
  if (!window_rect_changed && old_clip_was_empty == new_clip_is_empty)
    return;

  if (old_clip_was_empty != new_clip_is_empty) {
    PluginVisibilityChanged();
  }

  SetPluginRect(window_rect);

#ifndef NP_NO_QUICKDRAW
  if (window_rect_changed && qd_fast_path_enabled_) {
    // Pitch the old GWorlds, since they are the wrong size now; they will be
    // re-created on demand.
    UpdateGWorlds(NULL);
    // If the window size has changed, we need to turn off the fast path so that
    // the full redraw goes to the window and we get a correct baseline paint.
    SetQuickDrawFastPathEnabled(false);
    return;  // SetQuickDrawFastPathEnabled will call SetWindow for us.
  }
#endif

  WindowlessSetWindow(true);
}

void WebPluginDelegateImpl::DrawLayerInSurface() {
  surface_->MakeCurrent();

  surface_->Clear(window_rect_);

  [renderer_ beginFrameAtTime:CACurrentMediaTime() timeStamp:NULL];
  CGRect layerRect = [layer_ bounds];
  [renderer_ addUpdateRect:layerRect];
  [renderer_ render];
  [renderer_ endFrame];

  surface_->SwapBuffers();
  plugin_->AcceleratedFrameBuffersDidSwap(windowed_handle());
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

#ifndef NP_NO_QUICKDRAW
  if (instance()->drawing_model() == NPDrawingModelQuickDraw) {
    if (qd_fast_path_enabled_)
      SetGWorld(qd_port_.port, NULL);
    else
      SetPort(qd_port_.port);
  }
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

#ifndef NP_NO_QUICKDRAW
  if ((quirks_ & PLUGIN_QUIRK_ALLOW_FASTER_QUICKDRAW_PATH) &&
      !qd_fast_path_enabled_ && clip_rect_.IsEmpty()) {
    // Give the plugin a few seconds to stabilize so we get a good initial paint
    // to use as a baseline, then switch to the fast path.
    fast_path_enable_tick_ = base::TimeTicks::Now() +
        base::TimeDelta::FromSeconds(3);
  }
#endif

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

#ifndef NP_NO_QUICKDRAW
  // Make sure controls repaint with the correct look.
  if (qd_fast_path_enabled_)
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
    PluginVisibilityChanged();
    WindowlessSetWindow(true);
  }
}

// Update the size of the IOSurface to match the current size of the plug-in,
// then tell the browser host view so it can adjust its bookkeeping and CALayer
// appropriately.
void WebPluginDelegateImpl::UpdateAcceleratedSurface() {
  // Will only have a window handle when using the CoreAnimation drawing model.
  if (!windowed_handle() ||
      instance()->drawing_model() != NPDrawingModelCoreAnimation)
    return;

  [layer_ setFrame:CGRectMake(0, 0,
                              window_rect_.width(), window_rect_.height())];
  [renderer_ setBounds:[layer_ bounds]];

  uint64 io_surface_id = surface_->SetSurfaceSize(window_rect_.width(),
                                                 window_rect_.height());
  if (io_surface_id) {
    plugin_->SetAcceleratedSurface(windowed_handle(),
                                   window_rect_.width(),
                                   window_rect_.height(),
                                   io_surface_id);
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
    if (plugin_visible && !redraw_timer_->IsRunning()) {
      redraw_timer_->Start(
          base::TimeDelta::FromMilliseconds(kCoreAnimationRedrawPeriodMs),
          this, &WebPluginDelegateImpl::DrawLayerInSurface);
    } else if (!plugin_visible) {
      redraw_timer_->Stop();
    }
  }
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

void WebPluginDelegateImpl::CopyGWorldBits(GWorldPtr source, GWorldPtr dest) {
  if (!(source && dest))
    return;

  Rect window_bounds = { 0, 0, window_rect_.height(), window_rect_.width() };
  PixMapHandle source_pixmap = GetGWorldPixMap(source);
  if (LockPixels(source_pixmap)) {
    PixMapHandle dest_pixmap = GetGWorldPixMap(dest);
    if (LockPixels(dest_pixmap)) {
      SetGWorld(qd_buffer_world_, NULL);
      // Set foreground and background colors to avoid "colorizing" the image.
      ForeColor(blackColor);
      BackColor(whiteColor);
      CopyBits(reinterpret_cast<BitMap*>(*source_pixmap),
               reinterpret_cast<BitMap*>(*dest_pixmap),
               &window_bounds, &window_bounds, srcCopy, NULL);
      UnlockPixels(dest_pixmap);
    }
    UnlockPixels(source_pixmap);
  }
}

void WebPluginDelegateImpl::UpdateGWorlds(CGContextRef context) {
  if (qd_plugin_world_) {
    DisposeGWorld(qd_plugin_world_);
    qd_plugin_world_ = NULL;
  }
  if (qd_buffer_world_) {
    DisposeGWorld(qd_buffer_world_);
    qd_buffer_world_ = NULL;
  }
  if (!context)
    return;

  gfx::Size dimensions = window_rect_.size();
  Rect window_bounds = {
    0, 0, dimensions.height(), dimensions.width()
  };
  // Create a GWorld pointing at the same bits as our buffer context.
  if (context) {
    NewGWorldFromPtr(
        &qd_buffer_world_, k32BGRAPixelFormat, &window_bounds,
        NULL, NULL, 0, static_cast<Ptr>(CGBitmapContextGetData(context)),
        static_cast<SInt32>(CGBitmapContextGetBytesPerRow(context)));
  }
  // Create a GWorld for the plugin to paint into whenever it wants.
  NewGWorld(&qd_plugin_world_, k32ARGBPixelFormat, &window_bounds,
            NULL, NULL, kNativeEndianPixMap);
  if (qd_fast_path_enabled_)
    qd_port_.port = qd_plugin_world_;
}

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

  if (enabled == qd_fast_path_enabled_)
    return;
  if (enabled && clip_rect_.IsEmpty()) {
    // Don't switch to the fast path while the plugin is completely clipped;
    // we can only switch when the window has an up-to-date image for us to
    // scrape. We'll automatically switch after we become visible again.
    return;
  }

  qd_fast_path_enabled_ = enabled;
  if (enabled) {
    if (!qd_plugin_world_)
      UpdateGWorlds(buffer_context_);
    qd_port_.port = qd_plugin_world_;
    // Copy our last window snapshot into our new source, since the plugin
    // may not repaint everything.
    CopyGWorldBits(qd_buffer_world_, qd_plugin_world_);
  } else {
    qd_port_.port =
        GetWindowPort(reinterpret_cast<WindowRef>(np_cg_context_.window));
  }
  WindowlessSetWindow(true);
  // Send a paint event so that the new buffer gets updated immediately.
  WindowlessPaint(buffer_context_, clip_rect_);
}
#endif  // !NP_NO_QUICKDRAW

void WebPluginDelegateImpl::UpdateIdleEventRate() {
  bool plugin_visible = container_is_visible_ && !clip_rect_.IsEmpty();
  CarbonIdleEventSource::SharedInstance()->RegisterDelegate(this,
                                                            plugin_visible);
}
#endif  // !NP_NO_CARBON

// Returns the mask for just the button state in a WebInputEvent's modifiers.
static int WebEventButtonModifierMask() {
  return WebInputEvent::LeftButtonDown |
         WebInputEvent::RightButtonDown |
         WebInputEvent::MiddleButtonDown;
}

// Returns a new drag button state from applying |event| to the previous state.
static int UpdatedDragStateFromEvent(int drag_buttons,
                                     const WebInputEvent& event) {
  switch (event.type) {
    case WebInputEvent::MouseEnter:
      return event.modifiers & WebEventButtonModifierMask();
    case WebInputEvent::MouseUp: {
      const WebMouseEvent* mouse_event =
          static_cast<const WebMouseEvent*>(&event);
      int new_buttons = drag_buttons;
      if (mouse_event->button == WebMouseEvent::ButtonLeft)
        new_buttons &= ~WebInputEvent::LeftButtonDown;
      if (mouse_event->button == WebMouseEvent::ButtonMiddle)
        new_buttons &= ~WebInputEvent::MiddleButtonDown;
      if (mouse_event->button == WebMouseEvent::ButtonRight)
        new_buttons &= ~WebInputEvent::RightButtonDown;
      return new_buttons;
    }
    default:
      return drag_buttons;
  }
}

// Returns true if this is an event that looks like part of a drag with the
// given button state.
static bool EventIsRelatedToDrag(const WebInputEvent& event, int drag_buttons) {
  const WebMouseEvent* mouse_event = static_cast<const WebMouseEvent*>(&event);
  switch (event.type) {
    case WebInputEvent::MouseUp:
      // We only care about release of buttons that were part of the drag.
      return ((mouse_event->button == WebMouseEvent::ButtonLeft &&
               (drag_buttons & WebInputEvent::LeftButtonDown)) ||
              (mouse_event->button == WebMouseEvent::ButtonMiddle &&
               (drag_buttons & WebInputEvent::MiddleButtonDown)) ||
              (mouse_event->button == WebMouseEvent::ButtonRight &&
               (drag_buttons & WebInputEvent::RightButtonDown)));
    case WebInputEvent::MouseEnter:
      return (event.modifiers & WebEventButtonModifierMask()) != 0;
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseMove:
      return (drag_buttons &&
              drag_buttons == (event.modifiers & WebEventButtonModifierMask()));
    default:
      return false;
  }
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

  if (WebInputEvent::isMouseEventType(event.type)) {
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

      if (qd_fast_path_enabled_)
        SetGWorld(qd_port_.port, NULL);
      else
        SetPort(qd_port_.port);
    }
#endif

    if (event.type == WebInputEvent::MouseMove) {
      return true;  // The recurring FireIdleEvent will send null events.
    }
  }
#endif

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

  ScopedActiveDelegate active_delegate(this);

  // Create the plugin event structure.
  NPEventModel event_model = instance()->event_model();
  scoped_ptr<PluginWebEventConverter> event_converter(
      PluginWebEventConverterFactory::CreateConverterForModel(event_model));
  if (!(event_converter.get() && event_converter->InitWithEvent(event)))
    return false;
  void* plugin_event = event_converter->plugin_event();

  if (instance()->event_model() == NPEventModelCocoa) {
    // We recieve events related to drags starting outside the plugin, but the
    // NPAPI Cocoa event model spec says plugins shouldn't receive them, so
    // filter them out.
    // If we add a page capture mode at the WebKit layer (like the plugin
    // capture mode that handles drags starting inside) this can be removed.
    bool drag_related = EventIsRelatedToDrag(event, external_drag_buttons_);
    external_drag_buttons_ = UpdatedDragStateFromEvent(external_drag_buttons_,
                                                       event);
    if (drag_related) {
      if (event.type == WebInputEvent::MouseUp && !external_drag_buttons_) {
        // When an external drag ends, we need to synthesize a MouseEntered.
        NPCocoaEvent enter_event = *(static_cast<NPCocoaEvent*>(plugin_event));
        enter_event.type = NPCocoaEventMouseEntered;
        NPAPI::ScopedCurrentPluginEvent event_scope(instance(), &enter_event);
        instance()->NPP_HandleEvent(&enter_event);
      }
      return false;
    }
  }

  // Send the plugin the event.
  scoped_ptr<NPAPI::ScopedCurrentPluginEvent> event_scope(NULL);
  if (instance()->event_model() == NPEventModelCocoa) {
    event_scope.reset(new NPAPI::ScopedCurrentPluginEvent(
        instance(), static_cast<NPCocoaEvent*>(plugin_event)));
  }
  bool handled = instance()->NPP_HandleEvent(plugin_event) != 0;

  if (WebInputEvent::isMouseEventType(event.type)) {
    // Plugins are not good about giving accurate information about whether or
    // not they handled events, and other browsers on the Mac generally ignore
    // the return value. We may need to expand this to other input types, but
    // we'll need to be careful about things like Command-keys.
    handled = true;
  }

  return handled;
}

#ifndef NP_NO_CARBON
void WebPluginDelegateImpl::FireIdleEvent() {
  // Avoid a race condition between IO and UI threads during plugin shutdown
  if (!instance())
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
  if (instance()->drawing_model() == NPDrawingModelQuickDraw) {
    if (qd_fast_path_enabled_)
      SetGWorld(qd_port_.port, NULL);
    else
      SetPort(qd_port_.port);
  }
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
  // TODO: only do this if the contents of the offscreen window/buffer have
  // changed, so as not to spam the renderer with an unchanging image.
  if (instance() && instance()->drawing_model() == NPDrawingModelQuickDraw)
    instance()->webplugin()->Invalidate();
#endif
}
#endif  // !NP_NO_CARBON
