// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/web_content_controller.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "chromecast/browser/webview/webview_navigation_throttle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/web_preferences.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/events/event.h"

namespace chromecast {

WebContentController::WebContentController(Client* client) : client_(client) {
  js_channels_ = std::make_unique<WebContentJsChannels>(client_);
  JsClientInstance::AddObserver(this);
}

WebContentController::~WebContentController() {
  JsClientInstance::RemoveObserver(this);
  if (surface_) {
    surface_->RemoveSurfaceObserver(this);
    surface_->SetEmbeddedSurfaceId(base::RepeatingCallback<viz::SurfaceId()>());
  }
  if (!current_render_widget_set_.empty()) {
    // TODO(b/150955487): A WebContentController can be destructed without us
    // having received RenderViewDeleted notifications for all observed
    // RenderWidgetHosts, so we go through the current_render_widget_set_ to
    // remove the input event observers. It has sometimes been the case (perhaps
    // only on a renderer process crash; requires investigation) that an
    // observed RenderWidgetHost has disappeared without notification.
    // Therefore, it is not safe to call RemoveInputEventObserver on every
    // RenderWidgetHost that we started observing; we need to remove only
    // from currently live RenderWidgetHosts.
    std::unique_ptr<content::RenderWidgetHostIterator> widgets(
        content::RenderWidgetHost::GetRenderWidgetHosts());
    while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
      auto it = current_render_widget_set_.find(widget);
      if (it != current_render_widget_set_.end()) {
        widget->RemoveInputEventObserver(this);
      }
    }
  }
}

void WebContentController::ProcessRequest(
    const webview::WebviewRequest& request) {
  content::WebContents* contents = GetWebContents();
  switch (request.type_case()) {
    case webview::WebviewRequest::kInput:
      ProcessInputEvent(request.input());
      break;

    case webview::WebviewRequest::kEvaluateJavascript:
      if (request.has_evaluate_javascript()) {
        HandleEvaluateJavascript(request.id(), request.evaluate_javascript());
      } else {
        client_->OnError("evaluate_javascript() not supplied");
      }
      break;

    case webview::WebviewRequest::kAddJavascriptChannels:
      if (request.has_add_javascript_channels()) {
        HandleAddJavascriptChannels(request.add_javascript_channels());
      } else {
        client_->OnError("add_javascript_channels() not supplied");
      }
      break;

    case webview::WebviewRequest::kRemoveJavascriptChannels:
      if (request.has_remove_javascript_channels()) {
        HandleRemoveJavascriptChannels(request.remove_javascript_channels());
      } else {
        client_->OnError("remove_javascript_channels() not supplied");
      }
      break;

    case webview::WebviewRequest::kGetCurrentUrl:
      HandleGetCurrentUrl(request.id());
      break;

    case webview::WebviewRequest::kCanGoBack:
      HandleCanGoBack(request.id());
      break;

    case webview::WebviewRequest::kCanGoForward:
      HandleCanGoForward(request.id());
      break;

    case webview::WebviewRequest::kGoBack:
      contents->GetController().GoBack();
      break;

    case webview::WebviewRequest::kGoForward:
      contents->GetController().GoForward();
      break;

    case webview::WebviewRequest::kReload:
      // TODO(dnicoara): Are the default parameters correct?
      contents->GetController().Reload(content::ReloadType::NORMAL,
                                       /*check_for_repost=*/true);
      break;

    case webview::WebviewRequest::kClearCache:
      HandleClearCache();
      break;

    case webview::WebviewRequest::kGetTitle:
      HandleGetTitle(request.id());
      break;

    case webview::WebviewRequest::kSetAutoMediaPlaybackPolicy:
      if (request.has_set_auto_media_playback_policy()) {
        HandleSetAutoMediaPlaybackPolicy(
            request.set_auto_media_playback_policy());
      } else {
        client_->OnError("set_auto_media_playback_policy() not supplied");
      }
      break;

    case webview::WebviewRequest::kResize:
      if (request.has_resize()) {
        HandleResize(
            gfx::Size(request.resize().width(), request.resize().height()));
      } else {
        client_->OnError("resize() not supplied");
      }
      break;

    default:
      client_->OnError("Unknown request code");
      break;
  }
}

void WebContentController::AttachTo(aura::Window* window, int window_id) {
  content::WebContents* contents = GetWebContents();
  auto* contents_window = contents->GetNativeView();
  contents_window->set_id(window_id);
  // The aura window is hidden to avoid being shown via the usual layer method,
  // instead it is shows via a SurfaceDrawQuad by exo.
  contents_window->Hide();
  window->AddChild(contents_window);

  exo::Surface* surface = exo::Surface::AsSurface(window);
  CHECK(surface) << "Attaching Webview to non-EXO surface window";
  CHECK(!surface_) << "Attaching already attached WebView";

  surface_ = surface;
  surface_->AddSurfaceObserver(this);

  // Unretained is safe because we unset this in the destructor.
  surface_->SetEmbeddedSurfaceId(base::BindRepeating(
      &WebContentController::GetSurfaceId, base::Unretained(this)));
  HandleResize(contents_window->bounds().size());
}

void WebContentController::ProcessInputEvent(const webview::InputEvent& ev) {
  content::WebContents* contents = GetWebContents();
  DCHECK(contents);
  DCHECK(contents->GetNativeView());
  if (!contents->GetNativeView()->CanFocus())
    return;
  // Ensure this web contents has focus before sending it input.
  if (!contents->GetNativeView()->HasFocus())
    contents->GetNativeView()->Focus();

  content::RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView();
  ui::EventHandler* handler = rwhv->GetNativeView()->delegate();
  ui::EventType type = static_cast<ui::EventType>(ev.event_type());
  switch (type) {
    case ui::ET_TOUCH_RELEASED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_TOUCH_MOVED:
    case ui::ET_TOUCH_CANCELLED:
      if (ev.has_touch()) {
        auto& touch = ev.touch();
        ui::TouchEvent evt(
            type, gfx::PointF(touch.x(), touch.y()),
            gfx::PointF(touch.root_x(), touch.root_y()),
            base::TimeTicks() +
                base::TimeDelta::FromMicroseconds(ev.timestamp()),
            ui::PointerDetails(
                static_cast<ui::EventPointerType>(touch.pointer_type()),
                static_cast<ui::PointerId>(touch.pointer_id()),
                touch.radius_x(), touch.radius_y(), touch.force(),
                touch.twist(), touch.tilt_x(), touch.tilt_y(),
                touch.tangential_pressure()),
            ev.flags());

        ui::TouchEvent root_relative_event(evt);
        root_relative_event.set_location_f(evt.root_location_f());

        // GestureRecognizerImpl makes several APIs private so cast it to the
        // interface.
        ui::GestureRecognizer* recognizer = &gesture_recognizer_;

        // Run touches through the gesture recognition pipeline, web content
        // typically wants to process gesture events, not touch events.
        if (!recognizer->ProcessTouchEventPreDispatch(
                &root_relative_event, contents->GetNativeView())) {
          return;
        }
        // This flag is set depending on the gestures recognized in the call
        // above, and needs to propagate with the forwarded event.
        evt.set_may_cause_scrolling(root_relative_event.may_cause_scrolling());

        if (type == ui::ET_TOUCH_PRESSED) {
          // Ensure that we are observing the RenderWidgetHost for this touch
          // sequence, even if we didn't get a WebContentsObserver notification
          // for its creation. (This is not the normal case, but can happen
          // e.g. when loading a page with the Fling interface.)
          RegisterRenderWidgetInputObserver(rwhv->GetRenderWidgetHost());
        }

        // Record touch event information to match against acks.
        TouchData touch_data = {evt.unique_event_id(), rwhv, /*acked*/ false,
                                /*result*/ ui::ER_UNHANDLED};
        touch_queue_.push_back(touch_data);

        handler->OnTouchEvent(&evt);
      } else {
        client_->OnError("touch() not supplied for touch event");
      }
      break;
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSEWHEEL:
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      if (ev.has_mouse()) {
        auto& mouse = ev.mouse();
        ui::MouseEvent evt(
            type, gfx::PointF(mouse.x(), mouse.y()),
            gfx::PointF(mouse.root_x(), mouse.root_y()),
            base::TimeTicks() +
                base::TimeDelta::FromMicroseconds(ev.timestamp()),
            ev.flags(), mouse.changed_button_flags());
        if (contents->GetAccessibilityMode().has_mode(
                ui::AXMode::kWebContents)) {
          evt.set_flags(evt.flags() | ui::EF_TOUCH_ACCESSIBILITY);
        }
        handler->OnMouseEvent(&evt);
      } else {
        client_->OnError("mouse() not supplied for mouse event");
      }
      break;
    default:
      break;
  }
}

void WebContentController::RegisterRenderWidgetInputObserverFromRenderFrameHost(
    WebContentController* web_content_controller,
    content::RenderFrameHost* render_frame_host) {
  web_content_controller->RegisterRenderWidgetInputObserver(
      render_frame_host->GetView()->GetRenderWidgetHost());
}

void WebContentController::RegisterRenderWidgetInputObserver(
    content::RenderWidgetHost* render_widget_host) {
  auto insertion = current_render_widget_set_.insert(render_widget_host);
  if (insertion.second) {
    render_widget_host->AddInputEventObserver(this);
  }
}

void WebContentController::UnregisterRenderWidgetInputObserver(
    content::RenderWidgetHost* render_widget_host) {
  current_render_widget_set_.erase(render_widget_host);
  render_widget_host->RemoveInputEventObserver(this);
}

void WebContentController::JavascriptCallback(int64_t id, base::Value result) {
  std::string json;
  base::JSONWriter::Write(result, &json);
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  response->set_id(id);
  response->mutable_evaluate_javascript()->set_json(json);

  // Async response may come after Destroy() was called but before the web page
  // closed.
  if (client_)
    client_->EnqueueSend(std::move(response));
}

void WebContentController::HandleEvaluateJavascript(
    int64_t id,
    const webview::EvaluateJavascriptRequest& request) {
  GetWebContents()->GetMainFrame()->ExecuteJavaScript(
      base::UTF8ToUTF16(request.javascript_blob()),
      base::BindOnce(&WebContentController::JavascriptCallback,
                     weak_ptr_factory_.GetWeakPtr(), id));
}

void WebContentController::HandleAddJavascriptChannels(
    const webview::AddJavascriptChannelsRequest& request) {
  for (auto& channel : request.channels()) {
    current_javascript_channel_set_.insert(channel);
    for (auto* frame : current_render_frame_set_) {
      ChannelModified(frame, channel, true);
    }
  }
}

void WebContentController::HandleRemoveJavascriptChannels(
    const webview::RemoveJavascriptChannelsRequest& request) {
  for (auto& channel : request.channels()) {
    current_javascript_channel_set_.erase(channel);
    for (auto* frame : current_render_frame_set_) {
      ChannelModified(frame, channel, false);
    }
  }
}

void WebContentController::HandleGetCurrentUrl(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_get_current_url()->set_url(
      GetWebContents()->GetURL().spec());
  client_->EnqueueSend(std::move(response));
}

void WebContentController::HandleCanGoBack(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_can_go_back()->set_can_go_back(
      GetWebContents()->GetController().CanGoBack());
  client_->EnqueueSend(std::move(response));
}

void WebContentController::HandleCanGoForward(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_can_go_forward()->set_can_go_forward(
      GetWebContents()->GetController().CanGoForward());
  client_->EnqueueSend(std::move(response));
}

void WebContentController::HandleClearCache() {
  // TODO(dnicoara): See if there is a generic way to inform the renderer to
  // clear cache.
  // Android has a specific renderer message for this:
  // https://cs.chromium.org/chromium/src/android_webview/common/render_view_messages.h?rcl=65107121555167a3db39de5633c3297f7e861315&l=44

  // Remove disk cache.
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(
          GetWebContents()->GetBrowserContext());
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_CACHE,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
                      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB);
}

void WebContentController::HandleGetTitle(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_get_title()->set_title(
      base::UTF16ToUTF8(GetWebContents()->GetTitle()));
  client_->EnqueueSend(std::move(response));
}

void WebContentController::HandleSetAutoMediaPlaybackPolicy(
    const webview::SetAutoMediaPlaybackPolicyRequest& request) {
  content::WebContents* contents = GetWebContents();
  content::WebPreferences prefs =
      contents->GetRenderViewHost()->GetWebkitPreferences();
  prefs.autoplay_policy = request.require_user_gesture()
                              ? content::AutoplayPolicy::kUserGestureRequired
                              : content::AutoplayPolicy::kNoUserGestureRequired;
  contents->GetRenderViewHost()->UpdateWebkitPreferences(prefs);
}

void WebContentController::HandleResize(const gfx::Size& size) {
  LOG(INFO) << "Sizing web content to " << size.ToString();
  GetWebContents()->GetNativeView()->SetBounds(gfx::Rect(size));
  if (surface_) {
    surface_->SetEmbeddedSurfaceSize(size);
    surface_->Commit();
  }
}

viz::SurfaceId WebContentController::GetSurfaceId() {
  content::WebContents* web_contents = GetWebContents();
  // Web contents are destroyed before controller for cast apps.
  if (!web_contents)
    return viz::SurfaceId();
  auto* rwhv = web_contents->GetRenderWidgetHostView();
  if (!rwhv)
    return viz::SurfaceId();
  auto frame_sink_id = rwhv->GetRenderWidgetHost()->GetFrameSinkId();
  auto local_surface_id =
      rwhv->GetNativeView()->GetLocalSurfaceIdAllocation().local_surface_id();
  return viz::SurfaceId(frame_sink_id, local_surface_id);
}

void WebContentController::OnSurfaceDestroying(exo::Surface* surface) {
  DCHECK_EQ(surface, surface_);
  surface->RemoveSurfaceObserver(this);
  surface_ = nullptr;
}

void WebContentController::MainFrameWasResized(bool width_changed) {
  // The surface ID may have changed, so trigger a new commit to re-issue the
  // draw quad.
  if (surface_) {
    surface_->Commit();
  }
}

void WebContentController::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  // The surface ID may have changed, so trigger a new commit to re-issue the
  // draw quad.
  if (surface_) {
    surface_->Commit();
  }
}

void WebContentController::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  current_render_frame_set_.insert(render_frame_host);
  auto* instance =
      JsClientInstance::Find(render_frame_host->GetProcess()->GetID(),
                             render_frame_host->GetRoutingID());
  // If the instance doesn't exist yet the JsClientInstance observer will see
  // it later on.
  if (instance)
    SendInitialChannelSet(instance);
  RegisterRenderWidgetInputObserver(
      render_frame_host->GetView()->GetRenderWidgetHost());
}

void WebContentController::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  current_render_frame_set_.erase(render_frame_host);
  // The RenderFrameHost might not have a RenderWidgetHostView.
  // TODO(b/150955487): Investigate the conditions for this (renderer process
  // crash?) and Render.* relationships and notifications to see whether this
  // can be cleaned up (and in particular not potentially retain stale
  // |current_render_widget_set_| entries).
  content::RenderWidgetHostView* const rwhv = render_frame_host->GetView();
  if (rwhv) {
    UnregisterRenderWidgetInputObserver(rwhv->GetRenderWidgetHost());
  }
}

void WebContentController::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // The surface ID may have changed, so trigger a new commit to re-issue the
  // draw quad.
  if (surface_) {
    surface_->Commit();
  }
}

void WebContentController::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  RegisterRenderWidgetInputObserver(render_view_host->GetWidget());
}

void WebContentController::RenderViewDeleted(
    content::RenderViewHost* render_view_host) {
  content::RenderWidgetHost* rwh = render_view_host->GetWidget();
  UnregisterRenderWidgetInputObserver(rwh);
  content::RenderWidgetHostView* rwhv = rwh->GetView();
  base::EraseIf(touch_queue_,
                [rwhv](TouchData data) { return data.rwhv == rwhv; });
}

void WebContentController::OnJsClientInstanceRegistered(
    int process_id,
    int routing_id,
    JsClientInstance* instance) {
  if (current_render_frame_set_.find(content::RenderFrameHost::FromID(
          process_id, routing_id)) != current_render_frame_set_.end()) {
    // If the frame exists in the set then it cannot have been handled by
    // RenderFrameCreated.
    SendInitialChannelSet(instance);
  }
}

void WebContentController::AckTouchEvent(content::RenderWidgetHostView* rwhv,
                                         uint32_t unique_event_id,
                                         ui::EventResult result) {
  // GestureRecognizerImpl makes AckTouchEvent private so cast to the interface.
  ui::GestureRecognizer* recognizer = &gesture_recognizer_;
  auto list = recognizer->AckTouchEvent(unique_event_id, result, false,
                                        rwhv->GetNativeView());
  // Forward any resulting gestures.
  ui::EventHandler* handler = rwhv->GetNativeView()->delegate();
  for (auto& e : list) {
    handler->OnGestureEvent(e.get());
  }
}

void WebContentController::OnInputEventAck(
    blink::mojom::InputEventResultSource source,
    blink::mojom::InputEventResultState state,
    const blink::WebInputEvent& e) {
  if (!blink::WebInputEvent::IsTouchEventType(e.GetType()))
    return;
  const uint32_t id =
      static_cast<const blink::WebTouchEvent&>(e).unique_touch_event_id;
  ui::EventResult result =
      state == blink::mojom::InputEventResultState::kConsumed
          ? ui::ER_HANDLED
          : ui::ER_UNHANDLED;
  const auto it = find_if(touch_queue_.begin(), touch_queue_.end(),
                          [id](TouchData data) { return data.id == id; });
  if (it == touch_queue_.end()) {
    content::WebContents* contents = GetWebContents();
    content::RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView();
    AckTouchEvent(rwhv, id, result);
  } else {
    // Record the ack.
    it->acked = true;
    it->result = result;
    // Handle any available acks.
    while (!touch_queue_.empty() && touch_queue_.front().acked) {
      TouchData data = touch_queue_.front();
      touch_queue_.pop_front();
      AckTouchEvent(data.rwhv, data.id, data.result);
    }
  }
}

void WebContentController::ChannelModified(content::RenderFrameHost* frame,
                                           const std::string& channel,
                                           bool added) {
  auto* instance = JsClientInstance::Find(frame->GetProcess()->GetID(),
                                          frame->GetRoutingID());
  if (instance) {
    if (added) {
      instance->AddChannel(channel, GetJsChannelCallback());
    } else {
      instance->RemoveChannel(channel);
    }
  } else {
    LOG(WARNING) << "Cannot change channel " << channel << " for "
                 << frame->GetLastCommittedURL().possibly_invalid_spec();
  }
}

JsChannelCallback WebContentController::GetJsChannelCallback() {
  return base::BindRepeating(&WebContentJsChannels::SendMessage,
                             js_channels_->AsWeakPtr());
}

void WebContentController::SendInitialChannelSet(JsClientInstance* instance) {
  // Calls may come after Destroy() was called but before the web page closed.
  if (!js_channels_)
    return;

  JsChannelCallback callback = GetJsChannelCallback();
  for (auto& channel : current_javascript_channel_set_)
    instance->AddChannel(channel, callback);
}

WebContentJsChannels::WebContentJsChannels(WebContentController::Client* client)
    : client_(client) {}

WebContentJsChannels::~WebContentJsChannels() = default;

void WebContentJsChannels::SendMessage(const std::string& channel,
                                       const std::string& message) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  auto* js_message = response->mutable_javascript_channel_message();
  js_message->set_channel(channel);
  js_message->set_message(message);
  client_->EnqueueSend(std::move(response));
}

}  // namespace chromecast
