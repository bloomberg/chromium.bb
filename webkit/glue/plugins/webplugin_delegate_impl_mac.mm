// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "config.h"
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
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebCursorInfo;
using WebKit::WebKeyboardEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;

// Important implementation notes: The Mac definition of NPAPI, particularly
// the distinction between windowed and windowless modes, differs from the
// Windows and Linux definitions.  Most of those differences are
// accomodated by the WebPluginDelegate class.

namespace {

// The fastest we are willing to process idle events for Flash.
// Flash can easily exceed the limits of our CPU if we don't throttle it.
// The throttle has been chosen by testing various delays and compromising
// on acceptable Flash performance and reasonable CPU consumption.
//
// We'd like to make the throttle delay variable, based on the amount of
// time currently required to paint Flash plugins.  There isn't a good
// way to count the time spent in aggregate plugin painting, however, so
// this seems to work well enough.
const int kFlashIdleThrottleDelayMs = 20;  // 20ms (50Hz)

// The current instance of the plugin which entered the modal loop.
WebPluginDelegateImpl* g_current_plugin_instance = NULL;

}  // namespace

WebPluginDelegate* WebPluginDelegate::Create(
    const FilePath& filename,
    const std::string& mime_type,
    gfx::PluginWindowHandle containing_view) {
  scoped_refptr<NPAPI::PluginLib> plugin =
      NPAPI::PluginLib::CreatePluginLib(filename);
  if (plugin.get() == NULL)
    return NULL;

  NPError err = plugin->NP_Initialize();
  if (err != NPERR_NO_ERROR)
    return NULL;

  scoped_refptr<NPAPI::PluginInstance> instance =
      plugin->CreateInstance(mime_type);
  return new WebPluginDelegateImpl(containing_view, instance.get());
}

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
      user_gesture_msg_factory_(this) {
  memset(&window_, 0, sizeof(window_));
}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
  DestroyInstance();

  if (cg_context_.window)
    DisposeWindow(cg_context_.window);
}

void WebPluginDelegateImpl::PluginDestroyed() {
  delete this;
}

bool WebPluginDelegateImpl::Initialize(const GURL& url,
                                       char** argn,
                                       char** argv,
                                       int argc,
                                       WebPlugin* plugin,
                                       bool load_manually) {
  plugin_ = plugin;

  instance_->set_web_plugin(plugin);
  NPAPI::PluginInstance* old_instance =
      NPAPI::PluginInstance::SetInitializingInstance(instance_);


  bool start_result = instance_->Start(url, argn, argv, argc, load_manually);

  NPAPI::PluginInstance::SetInitializingInstance(old_instance);

  if (!start_result)
    return false;

  cg_context_.window = NULL;
  window_.window = &cg_context_;
  window_.type = NPWindowTypeDrawable;

  plugin->SetWindow(NULL);
  plugin_url_ = url.spec();

  return true;
}

void WebPluginDelegateImpl::DestroyInstance() {
  if (instance_ && (instance_->npp()->ndata != NULL)) {
    // Shutdown all streams before destroying so that
    // no streams are left "in progress".  Need to do
    // this before calling set_web_plugin(NULL) because the
    // instance uses the helper to do the download.
    instance_->CloseStreams();
    instance_->NPP_Destroy();
    instance_->set_web_plugin(NULL);
    instance_ = 0;
  }
}

void WebPluginDelegateImpl::UpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {

  if (!window_rect.IsEmpty()) {
    NSPoint windowOffset = {0.0, 0.0};
    Rect window_bounds;
    window_bounds.top = window_rect.y() + windowOffset.y;
    window_bounds.left = window_rect.x() + windowOffset.x;
    window_bounds.bottom = window_rect.y() + window_rect.height();
    window_bounds.right = window_rect.x() + window_rect.width();

    if (!cg_context_.window) {
      // For all plugins we create a placeholder offscreen window for the use
      // of NPWindow.  NPAPI on the Mac requires a Carbon WindowRef for the
      // "browser window", even if we're not using the Quickdraw drawing model.
      // Not having a valid window reference causes subtle bugs with plugins
      // which retreive the NPWindow and validate the same. The NPWindow
      // can be retreived via NPN_GetValue of NPNVnetscapeWindow.

      WindowRef window_ref;
      if (CreateNewWindow(kDocumentWindowClass,
                          kWindowStandardDocumentAttributes,
                          &window_bounds,
                          &window_ref) == noErr) {
        cg_context_.window = window_ref;
      }
    } else {
      SetWindowBounds(cg_context_.window, kWindowContentRgn, &window_bounds);
    }
  }

  WindowlessUpdateGeometry(window_rect, clip_rect);
}

void WebPluginDelegateImpl::Paint(CGContextRef context, const gfx::Rect& rect) {
  if (windowless_)
    WindowlessPaint(context, rect);
}

void WebPluginDelegateImpl::Print(CGContextRef context) {
  // Disabling the call to NPP_Print as it causes a crash in
  // flash in some cases. In any case this does not work as expected
  // as the EMF meta file dc passed in needs to be created with the
  // the plugin window dc as its sibling dc and the window rect
  // in .01 mm units.
}

NPObject* WebPluginDelegateImpl::GetPluginScriptableObject() {
  return instance_->GetPluginScriptableObject();
}

void WebPluginDelegateImpl::DidFinishLoadWithReason(NPReason reason) {
  instance()->DidFinishLoadWithReason(reason);
}

int WebPluginDelegateImpl::GetProcessId() {
  // We are in process, so the plugin pid is this current process pid.
  return getpid();
}

void WebPluginDelegateImpl::SendJavaScriptStream(const std::string& url,
                                                 const std::wstring& result,
                                                 bool success,
                                                 bool notify_needed,
                                                 intptr_t notify_data) {
  instance()->SendJavaScriptStream(url, result, success, notify_needed,
                                   notify_data);
}

void WebPluginDelegateImpl::DidReceiveManualResponse(
    const std::string& url, const std::string& mime_type,
    const std::string& headers, uint32 expected_length, uint32 last_modified) {
  instance()->DidReceiveManualResponse(url, mime_type, headers,
                                       expected_length, last_modified);
}

void WebPluginDelegateImpl::DidReceiveManualData(const char* buffer,
                                                 int length) {
  instance()->DidReceiveManualData(buffer, length);
}

void WebPluginDelegateImpl::DidFinishManualLoading() {
  instance()->DidFinishManualLoading();
}

void WebPluginDelegateImpl::DidManualLoadFail() {
  instance()->DidManualLoadFail();
}

FilePath WebPluginDelegateImpl::GetPluginPath() {
  return instance()->plugin_lib()->plugin_info().path;
}

void WebPluginDelegateImpl::InstallMissingPlugin() {
  NPEvent evt;
  instance()->NPP_HandleEvent(&evt);
}

void WebPluginDelegateImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  // Only resend to the instance if the geometry has changed.
  if (window_rect == window_rect_ && clip_rect == clip_rect_)
    return;

  // We will inform the instance of this change when we call NPP_SetWindow.
  clip_rect_ = clip_rect;
  cutout_rects_.clear();

  if (window_rect_ != window_rect) {
    window_rect_ = window_rect;

    window_.clipRect.top = clip_rect_.y();
    window_.clipRect.left = clip_rect_.x();
    window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
    window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
    window_.height = window_rect_.height();
    window_.width = window_rect_.width();
    window_.x = window_rect_.x();
    window_.y = window_rect_.y();
    window_.type = NPWindowTypeDrawable;
    windowless_needs_set_window_ = true;
  }
}

void WebPluginDelegateImpl::WindowlessPaint(gfx::NativeDrawingContext context,
                                            const gfx::Rect& damage_rect) {
  static StatsRate plugin_paint("Plugin.Paint");
  StatsScope<StatsRate> scope(plugin_paint);

  // We save and restore the NSGraphicsContext state in case the plugin uses
  // Cocoa drawing.
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:[NSGraphicsContext
                                        graphicsContextWithGraphicsPort:context
                                        flipped:NO]];

  CGContextSaveGState(context);
  cg_context_.context = context;
  window_.window = &cg_context_;
  window_.type = NPWindowTypeDrawable;
  if (windowless_needs_set_window_) {
    Rect window_bounds;
    window_bounds.top = window_rect_.y();
    window_bounds.left = window_rect_.x();
    window_bounds.bottom = window_rect_.y() + window_rect_.height();
    window_bounds.right = window_rect_.x() + window_rect_.width();
    if (!cg_context_.window) {
      // For all plugins we create a placeholder offscreen window for the use
      // of NPWindow.  NPAPI on the Mac requires a Carbon WindowRef for the
      // "browser window", even if we're not using the Quickdraw drawing model.
      // Not having a valid window reference causes subtle bugs with plugins
      // which retreive the NPWindow and validate the same. The NPWindow
      // can be retreived via NPN_GetValue of NPNVnetscapeWindow.

      WindowRef window_ref;
      if (CreateNewWindow(kDocumentWindowClass,
                          kWindowStandardDocumentAttributes,
                          &window_bounds,
                          &window_ref) == noErr) {
        cg_context_.window = window_ref;
      }
    } else {
      SetWindowBounds(cg_context_.window, kWindowContentRgn, &window_bounds);
    }
    instance()->NPP_SetWindow(&window_);
    windowless_needs_set_window_ = false;
  }
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

void WebPluginDelegateImpl::WindowlessSetWindow(bool force_set_window) {
  if (!instance())
    return;

  if (window_rect_.IsEmpty())  // wait for geometry to be set.
    return;

  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();
  window_.type = NPWindowTypeDrawable;

  if (!force_set_window)
    // Reset this flag before entering the instance in case of side-effects.
    windowless_needs_set_window_ = false;

  Rect window_bounds;
  window_bounds.top = window_rect_.y();
  window_bounds.left = window_rect_.x();
  window_bounds.bottom = window_rect_.y() + window_rect_.height();
  window_bounds.right = window_rect_.x() + window_rect_.width();
  if (!cg_context_.window) {
    // For all plugins we create a placeholder offscreen window for the use
    // of NPWindow.  NPAPI on the Mac requires a Carbon WindowRef for the
    // "browser window", even if we're not using the Quickdraw drawing model.
    // Not having a valid window reference causes subtle bugs with plugins
    // which retreive the NPWindow and validate the same. The NPWindow
    // can be retreived via NPN_GetValue of NPNVnetscapeWindow.

    WindowRef window_ref;
    if (CreateNewWindow(kDocumentWindowClass,
                        kWindowStandardDocumentAttributes,
                        &window_bounds,
                        &window_ref) == noErr) {
      cg_context_.window = window_ref;
    }
  } else {
    SetWindowBounds(cg_context_.window, kWindowContentRgn, &window_bounds);
  }

  NPError err = instance()->NPP_SetWindow(&window_);
  DCHECK(err == NPERR_NO_ERROR);
}

void WebPluginDelegateImpl::SetFocus() {
  NPEvent focus_event = { 0 };
  focus_event.what = NPEventType_GetFocusEvent;
  focus_event.when = TickCount();
  instance()->NPP_HandleEvent(&focus_event);
}

static bool NPEventFromWebMouseEvent(const WebMouseEvent& event,
                                     NPEvent *np_event) {
  np_event->where.h = event.windowX;
  np_event->where.v = event.windowY;

  if (event.modifiers & WebInputEvent::ControlKey)
    np_event->modifiers |= controlKey;
  if (event.modifiers & WebInputEvent::ShiftKey)
    np_event->modifiers |= shiftKey;

  switch (event.type) {
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseEnter:
      np_event->what = NPEventType_AdjustCursorEvent;
      return true;
    case WebInputEvent::MouseDown:
      switch (event.button) {
        case WebMouseEvent::ButtonLeft:
        case WebMouseEvent::ButtonMiddle:
        case WebMouseEvent::ButtonRight:
          np_event->what = mouseDown;
          break;
      }
      return true;
    case WebInputEvent::MouseUp:
      switch (event.button) {
        case WebMouseEvent::ButtonLeft:
        case WebMouseEvent::ButtonMiddle:
        case WebMouseEvent::ButtonRight:
          np_event->what = mouseUp;
          break;
      }
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool NPEventFromWebKeyboardEvent(const WebKeyboardEvent& event,
                                        NPEvent *np_event) {
  np_event->message = event.nativeKeyCode;

  switch (event.type) {
    case WebInputEvent::KeyDown:
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
      return NPEventFromWebMouseEvent(*static_cast<const WebMouseEvent*>(&event), np_event);
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
      if (event.size < sizeof(WebKeyboardEvent)) {
        NOTREACHED();
        return false;
      }
      return NPEventFromWebKeyboardEvent(*static_cast<const WebKeyboardEvent*>(&event), np_event);
    default:
      DLOG(WARNING) << "unknown event type" << event.type;
      return false;
  }
}

bool WebPluginDelegateImpl::HandleInputEvent(const WebInputEvent& event,
                                             WebCursorInfo* cursor) {
  DCHECK(windowless_) << "events should only be received in windowless mode";
  DCHECK(cursor != NULL);

  NPEvent np_event = {0};
  if (!NPEventFromWebInputEvent(event, &np_event)) {
    return false;
  }
  np_event.when = TickCount();
  bool ret = instance()->NPP_HandleEvent(&np_event) != 0;
  return ret;
}

WebPluginResourceClient* WebPluginDelegateImpl::CreateResourceClient(
    int resource_id, const std::string &url, bool notify_needed,
    intptr_t notify_data, intptr_t existing_stream) {
  // Stream already exists. This typically happens for range requests
  // initiated via NPN_RequestRead.
  if (existing_stream) {
    NPAPI::PluginStream* plugin_stream =
        reinterpret_cast<NPAPI::PluginStream*>(existing_stream);

    plugin_stream->CancelRequest();

    return plugin_stream->AsResourceClient();
  }

  if (notify_needed) {
    instance()->SetURLLoadData(GURL(url.c_str()), notify_data);
  }
  std::string mime_type;
  NPAPI::PluginStreamUrl *stream = instance()->CreateStream(
      resource_id, url, mime_type, notify_needed,
      reinterpret_cast<void*>(notify_data));
  return stream;
}

void WebPluginDelegateImpl::URLRequestRouted(const std::string&url,
                                             bool notify_needed,
                                             intptr_t notify_data) {
  if (notify_needed) {
    instance()->SetURLLoadData(GURL(url.c_str()), notify_data);
  }
}
