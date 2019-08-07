/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Google Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/frame/local_frame.h"

#include <limits>
#include <memory>
#include <utility>

#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_content_capture_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/child_frame_disconnector.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/ignore_opens_during_unload_count_incrementer.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_controller.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/frame_overlay.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/loader/navigation_scheduler.h"
#include "third_party/blink/renderer/core/loader/previews_resource_loading_hints_receiver_impl.h"
#include "third_party/blink/renderer/core/page/drag_controller.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/core/page/plugin_script_forbidden_scope.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/paint/compositing/graphics_layer_tree_as_text.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/frame_resource_coordinator.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

inline float ParentPageZoomFactor(LocalFrame* frame) {
  auto* parent_local_frame = DynamicTo<LocalFrame>(frame->Tree().Parent());
  return parent_local_frame ? parent_local_frame->PageZoomFactor() : 1;
}

inline float ParentTextZoomFactor(LocalFrame* frame) {
  auto* parent_local_frame = DynamicTo<LocalFrame>(frame->Tree().Parent());
  return parent_local_frame ? parent_local_frame->TextZoomFactor() : 1;
}

bool ShouldUseClientLoFiForRequest(
    const ResourceRequest& request,
    WebURLRequest::PreviewsState frame_previews_state) {
  if (request.GetPreviewsState() != WebURLRequest::kPreviewsUnspecified)
    return request.GetPreviewsState() & WebURLRequest::kClientLoFiOn;

  if (!(frame_previews_state & WebURLRequest::kClientLoFiOn))
    return false;

  // Even if this frame is using Server Lo-Fi, https:// images won't be
  // handled by Server Lo-Fi since their requests won't be sent to the Data
  // Saver proxy, so use Client Lo-Fi instead.
  if (frame_previews_state & WebURLRequest::kServerLoFiOn)
    return request.Url().ProtocolIs("https");

  return true;
}

WeakPersistent<LocalFrame>& UserActivationNotifierFrame() {
  DEFINE_STATIC_LOCAL(WeakPersistent<LocalFrame>,
                      user_activation_notifier_frame, (nullptr));
  return user_activation_notifier_frame;
}

enum class UserActivationFrameResultEnum : int {
  kNullFailure = 0,
  kNullSuccess = 1,
  kSelfFailure = 2,
  kSelfSuccess = 3,
  kAncestorFailure = 4,
  kAncestorSuccess = 5,
  kDescendantFailure = 6,
  kDescendantSuccess = 7,
  kOtherFailure = 8,
  kOtherSuccess = 9,
  kNonMainThreadFailure = 10,
  kNonMainThreadSuccess = 11,
  kMaxValue = kNonMainThreadSuccess
};

UserActivationFrameResultEnum DetermineActivationResultEnum(
    const LocalFrame* const caller_frame,
    const bool call_succeeded,
    const bool off_main_thread) {
  if (off_main_thread) {
    return call_succeeded
               ? UserActivationFrameResultEnum::kNonMainThreadSuccess
               : UserActivationFrameResultEnum::kNonMainThreadFailure;
  }

  LocalFrame* user_activation_notifier_frame = UserActivationNotifierFrame();

  if (!caller_frame || !user_activation_notifier_frame) {
    return call_succeeded ? UserActivationFrameResultEnum::kNullSuccess
                          : UserActivationFrameResultEnum::kNullFailure;
  }
  if (caller_frame == user_activation_notifier_frame) {
    return call_succeeded ? UserActivationFrameResultEnum::kSelfSuccess
                          : UserActivationFrameResultEnum::kSelfFailure;
  }
  if (user_activation_notifier_frame->Tree().IsDescendantOf(caller_frame)) {
    return call_succeeded ? UserActivationFrameResultEnum::kAncestorSuccess
                          : UserActivationFrameResultEnum::kAncestorFailure;
  }
  if (caller_frame->Tree().IsDescendantOf(user_activation_notifier_frame)) {
    return call_succeeded ? UserActivationFrameResultEnum::kDescendantSuccess
                          : UserActivationFrameResultEnum::kDescendantFailure;
  }
  return call_succeeded ? UserActivationFrameResultEnum::kOtherSuccess
                        : UserActivationFrameResultEnum::kOtherFailure;
}

}  // namespace

template class CORE_TEMPLATE_EXPORT Supplement<LocalFrame>;

// static
LocalFrame* LocalFrame::Create(LocalFrameClient* client,
                               Page& page,
                               FrameOwner* owner,
                               InterfaceRegistry* interface_registry) {
  LocalFrame* frame = MakeGarbageCollected<LocalFrame>(
      client, page, owner,
      interface_registry ? interface_registry
                         : InterfaceRegistry::GetEmptyInterfaceRegistry());
  frame->Initialize();

  probe::FrameAttachedToParent(frame);
  return frame;
}

void LocalFrame::Init() {
  CoreInitializer::GetInstance().InitLocalFrame(*this);

  loader_.Init();
}

void LocalFrame::SetView(LocalFrameView* view) {
  DCHECK(!view_ || view_ != view);
  DCHECK(!GetDocument() || !GetDocument()->IsActive());
  if (view_)
    view_->WillBeRemovedFromFrame();
  view_ = view;
}

void LocalFrame::CreateView(const IntSize& viewport_size,
                            const Color& background_color) {
  DCHECK(this);
  DCHECK(GetPage());

  bool is_local_root = this->IsLocalRoot();

  if (is_local_root && View())
    View()->SetParentVisible(false);

  SetView(nullptr);

  LocalFrameView* frame_view = nullptr;
  if (is_local_root) {
    frame_view = LocalFrameView::Create(*this, viewport_size);

    // The layout size is set by WebViewImpl to support @viewport
    frame_view->SetLayoutSizeFixedToFrameSize(false);
  } else {
    frame_view = LocalFrameView::Create(*this);
  }

  SetView(frame_view);

  frame_view->UpdateBaseBackgroundColorRecursively(background_color);

  if (is_local_root)
    frame_view->SetParentVisible(true);

  // FIXME: Not clear what the right thing for OOPI is here.
  if (OwnerLayoutObject()) {
    HTMLFrameOwnerElement* owner = DeprecatedLocalOwner();
    DCHECK(owner);
    // FIXME: OOPI might lead to us temporarily lying to a frame and telling it
    // that it's owned by a FrameOwner that knows nothing about it. If we're
    // lying to this frame, don't let it clobber the existing
    // EmbeddedContentView.
    if (owner->ContentFrame() == this)
      owner->SetEmbeddedContentView(frame_view);
  }

  if (Owner())
    View()->SetCanHaveScrollbars(Owner()->ScrollingMode() !=
                                 kScrollbarAlwaysOff);
}

LocalFrame::~LocalFrame() {
  // Verify that the LocalFrameView has been cleared as part of detaching
  // the frame owner.
  DCHECK(!view_);
  if (IsAdSubframe())
    InstanceCounters::DecrementCounter(InstanceCounters::kAdSubframeCounter);
}

void LocalFrame::Trace(blink::Visitor* visitor) {
  visitor->Trace(ad_tracker_);
  visitor->Trace(probe_sink_);
  visitor->Trace(performance_monitor_);
  visitor->Trace(idleness_detector_);
  visitor->Trace(inspector_trace_events_);
  visitor->Trace(loader_);
  visitor->Trace(navigation_scheduler_);
  visitor->Trace(view_);
  visitor->Trace(dom_window_);
  visitor->Trace(page_popup_owner_);
  visitor->Trace(script_controller_);
  visitor->Trace(editor_);
  visitor->Trace(spell_checker_);
  visitor->Trace(selection_);
  visitor->Trace(event_handler_);
  visitor->Trace(console_);
  visitor->Trace(input_method_controller_);
  visitor->Trace(text_suggestion_controller_);
  visitor->Trace(smooth_scroll_sequencer_);
  visitor->Trace(content_capture_manager_);
  Frame::Trace(visitor);
  Supplementable<LocalFrame>::Trace(visitor);
}

bool LocalFrame::IsLocalRoot() const {
  if (!Tree().Parent())
    return true;

  return Tree().Parent()->IsRemoteFrame();
}

void LocalFrame::ScheduleNavigation(Document& origin_document,
                                    const KURL& url,
                                    WebFrameLoadType frame_load_type,
                                    UserGestureStatus user_gesture_status) {
  if (!navigation_rate_limiter().CanProceed())
    return;
  navigation_scheduler_->ScheduleFrameNavigation(&origin_document, url,
                                                 frame_load_type);
}

void LocalFrame::Navigate(const FrameLoadRequest& request,
                          WebFrameLoadType frame_load_type) {
  if (!navigation_rate_limiter().CanProceed())
    return;
  if (request.ClientRedirectReason() != ClientNavigationReason::kNone) {
    probe::FrameScheduledNavigation(this, request.GetResourceRequest().Url(),
                                    0.0, request.ClientRedirectReason());
    if (NavigationScheduler::MustReplaceCurrentItem(this))
      frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
  }
  loader_.StartNavigation(request, frame_load_type);

  if (request.ClientRedirectReason() != ClientNavigationReason::kNone)
    probe::FrameClearedScheduledNavigation(this);
}

void LocalFrame::DetachImpl(FrameDetachType type) {
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // BEGIN RE-ENTRANCY SAFE BLOCK
  // Starting here, the code must be safe against re-entrancy. Dispatching
  // events, et cetera can run Javascript, which can reenter Detach().
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  if (this == UserActivationNotifierFrame())
    UserActivationNotifierFrame().Clear();

  frame_color_overlay_.reset();

  if (IsLocalRoot()) {
    performance_monitor_->Shutdown();
    if (ad_tracker_)
      ad_tracker_->Shutdown();
  }
  idleness_detector_->Shutdown();
  if (inspector_trace_events_)
    probe_sink_->RemoveInspectorTraceEvents(inspector_trace_events_);
  inspector_task_runner_->Dispose();

  PluginScriptForbiddenScope forbid_plugin_destructor_scripting;
  loader_.StopAllLoaders();
  // Don't allow any new child frames to load in this frame: attaching a new
  // child frame during or after detaching children results in an attached
  // frame on a detached DOM tree, which is bad.
  SubframeLoadingDisabler disabler(*GetDocument());
  // https://html.spec.whatwg.org/C/browsing-the-web.html#unload-a-document
  // The ignore-opens-during-unload counter of a Document must be incremented
  // both when unloading itself and when unloading its descendants.
  IgnoreOpensDuringUnloadCountIncrementer ignore_opens_during_unload(
      GetDocument());
  loader_.DispatchUnloadEvent();
  DetachChildren();

  // All done if detaching the subframes brought about a detach of this frame
  // also.
  if (!Client())
    return;

  // Detach() needs to be called after detachChildren(), because
  // detachChildren() will trigger the unload event handlers of any child
  // frames, and those event handlers might start a new subresource load in this
  // frame which should be stopped by Detach.
  loader_.Detach();
  GetDocument()->Shutdown();

  if (content_capture_manager_) {
    content_capture_manager_->Shutdown();
    content_capture_manager_ = nullptr;
  }

  // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout crashes.
  // It seems to crash because Frame is detached before LocalFrameView.
  // Verify here that any LocalFrameView has been detached by now.
  if (view_ && view_->IsAttached()) {
    CHECK(DeprecatedLocalOwner());
    CHECK(DeprecatedLocalOwner()->OwnedEmbeddedContentView());
    CHECK_EQ(view_, DeprecatedLocalOwner()->OwnedEmbeddedContentView());
  }
  CHECK(!view_ || !view_->IsAttached());

  // This is the earliest that scripting can be disabled:
  // - FrameLoader::Detach() can fire XHR abort events
  // - Document::Shutdown() can dispose plugins which can run script.
  ScriptForbiddenScope forbid_script;
  if (!Client())
    return;

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // END RE-ENTRANCY SAFE BLOCK
  // Past this point, no script should be executed. If this method was
  // re-entered, then check for a non-null Client() above should have already
  // returned.
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  DCHECK(!IsDetached());

  // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout crashes.
  CHECK(!view_->IsAttached());
  Client()->WillBeDetached();
  // Notify ScriptController that the frame is closing, since its cleanup ends
  // up calling back to LocalFrameClient via WindowProxy.
  GetScriptController().ClearForClose();

  // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout crashes.
  CHECK(!view_->IsAttached());
  SetView(nullptr);

  GetEventHandlerRegistry().DidRemoveAllEventHandlers(*DomWindow());

  DomWindow()->FrameDestroyed();

  probe::FrameDetachedFromParent(this);

  supplements_.clear();
  frame_scheduler_.reset();
  WeakIdentifierMap<LocalFrame>::NotifyObjectDestroyed(this);
}

bool LocalFrame::PrepareForCommit() {
  return Loader().PrepareForCommit();
}

void LocalFrame::CheckCompleted() {
  GetDocument()->CheckCompleted();
}

SecurityContext* LocalFrame::GetSecurityContext() const {
  return GetDocument();
}

void LocalFrame::PrintNavigationErrorMessage(const Frame& target_frame,
                                             const char* reason) {
  // URLs aren't available for RemoteFrames, so the error message uses their
  // origin instead.
  auto* target_local_frame = DynamicTo<LocalFrame>(&target_frame);
  String target_frame_description =
      target_local_frame
          ? "with URL '" +
                target_local_frame->GetDocument()->Url().GetString() + "'"
          : "with origin '" +
                target_frame.GetSecurityContext()
                    ->GetSecurityOrigin()
                    ->ToString() +
                "'";
  String message =
      "Unsafe JavaScript attempt to initiate navigation for frame " +
      target_frame_description + " from frame with URL '" +
      GetDocument()->Url().GetString() + "'. " + reason + "\n";

  DomWindow()->PrintErrorMessage(message);
}

void LocalFrame::PrintNavigationWarning(const String& message) {
  console_->AddMessage(
      ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                             mojom::ConsoleMessageLevel::kWarning, message));
}

bool LocalFrame::ShouldClose() {
  // TODO(dcheng): This should be fixed to dispatch beforeunload events to
  // both local and remote frames.
  return loader_.ShouldClose();
}

void LocalFrame::DetachChildren() {
  DCHECK(loader_.StateMachine()->CreatingInitialEmptyDocument() ||
         GetDocument());

  if (Document* document = this->GetDocument())
    ChildFrameDisconnector(*document).Disconnect();
}

void LocalFrame::DidAttachDocument() {
  Document* document = GetDocument();
  DCHECK(document);
  GetEditor().Clear();
  // Clearing the event handler clears many events, but notably can ensure that
  // for a drag started on an element in a frame that was moved (likely via
  // appendChild()), the drag source will detach and stop firing drag events
  // even after the frame reattaches.
  GetEventHandler().Clear();
  Selection().DidAttachDocument(document);
  GetInputMethodController().DidAttachDocument(document);
  GetSpellChecker().DidAttachDocument(document);
  GetTextSuggestionController().DidAttachDocument(document);
  previews_resource_loading_hints_receiver_.reset();
}

void LocalFrame::Reload(WebFrameLoadType load_type) {
  DCHECK(IsReloadLoadType(load_type));
  if (!loader_.GetDocumentLoader()->GetHistoryItem())
    return;
  FrameLoadRequest request = FrameLoadRequest(
      nullptr, loader_.ResourceRequestForReload(
                   load_type, ClientRedirectPolicy::kClientRedirect));
  request.SetClientRedirectReason(ClientNavigationReason::kReload);
  probe::FrameScheduledNavigation(this, request.GetResourceRequest().Url(), 0.0,
                                  ClientNavigationReason::kReload);
  loader_.StartNavigation(request, load_type);
  probe::FrameClearedScheduledNavigation(this);
}

LocalWindowProxy* LocalFrame::WindowProxy(DOMWrapperWorld& world) {
  return ToLocalWindowProxy(Frame::GetWindowProxy(world));
}

LocalDOMWindow* LocalFrame::DomWindow() const {
  return To<LocalDOMWindow>(dom_window_.Get());
}

void LocalFrame::SetDOMWindow(LocalDOMWindow* dom_window) {
  if (dom_window)
    GetScriptController().ClearWindowProxy();

  if (this->DomWindow())
    this->DomWindow()->Reset();
  dom_window_ = dom_window;
}

Document* LocalFrame::GetDocument() const {
  return DomWindow() ? DomWindow()->document() : nullptr;
}

void LocalFrame::SetPagePopupOwner(Element& owner) {
  page_popup_owner_ = &owner;
}

LayoutView* LocalFrame::ContentLayoutObject() const {
  return GetDocument() ? GetDocument()->GetLayoutView() : nullptr;
}

void LocalFrame::DidChangeVisibilityState() {
  if (GetDocument())
    GetDocument()->DidChangeVisibilityState();

  Frame::DidChangeVisibilityState();
}

void LocalFrame::DidFreeze() {
  DCHECK(RuntimeEnabledFeatures::PageLifecycleEnabled());
  if (GetDocument()) {
    auto* frame_resource_coordinator = GetFrameResourceCoordinator();
    if (frame_resource_coordinator) {
      // Determine if there is a beforeunload handler by dispatching a
      // beforeunload that will *not* launch a user dialog. If
      // |proceed| is false then there is a non-empty beforeunload
      // handler indicating potentially unsaved user state.
      bool unused_did_allow_navigation = false;
      bool proceed = GetDocument()->DispatchBeforeUnloadEvent(
          nullptr, false /* is_reload */, unused_did_allow_navigation);
      frame_resource_coordinator->SetHasNonEmptyBeforeUnload(!proceed);
    }

    GetDocument()->DispatchFreezeEvent();
    // TODO(fmeawad): Move the following logic to the page once we have a
    // PageResourceCoordinator in Blink. http://crbug.com/838415
    if (frame_resource_coordinator) {
      frame_resource_coordinator->SetLifecycleState(
          resource_coordinator::mojom::LifecycleState::kFrozen);
    }
  }
}

void LocalFrame::DidResume() {
  DCHECK(RuntimeEnabledFeatures::PageLifecycleEnabled());
  if (GetDocument()) {
    const TimeTicks resume_event_start = CurrentTimeTicks();
    GetDocument()->DispatchEvent(*Event::Create(event_type_names::kResume));
    const TimeTicks resume_event_end = CurrentTimeTicks();
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, resume_histogram,
        ("DocumentEventTiming.ResumeDuration", 0, 10000000, 50));
    resume_histogram.CountMicroseconds(resume_event_end - resume_event_start);
    // TODO(fmeawad): Move the following logic to the page once we have a
    // PageResourceCoordinator in Blink
    if (auto* frame_resource_coordinator = GetFrameResourceCoordinator()) {
      frame_resource_coordinator->SetLifecycleState(
          resource_coordinator::mojom::LifecycleState::kRunning);
    }
  }
}

void LocalFrame::SetIsInert(bool inert) {
  is_inert_ = inert;
  PropagateInertToChildFrames();
}

void LocalFrame::PropagateInertToChildFrames() {
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    // is_inert_ means that this Frame is inert because of a modal dialog or
    // inert element in an ancestor Frame. Otherwise, decide whether a child
    // Frame element is inert because of an element in this Frame.
    child->SetIsInert(is_inert_ ||
                      To<HTMLFrameOwnerElement>(child->Owner())->IsInert());
  }
}

void LocalFrame::SetInheritedEffectiveTouchAction(TouchAction touch_action) {
  if (inherited_effective_touch_action_ == touch_action)
    return;
  inherited_effective_touch_action_ = touch_action;
  if (GetDocument()->documentElement()) {
    GetDocument()->documentElement()->SetNeedsStyleRecalc(
        kSubtreeStyleChange,
        StyleChangeReasonForTracing::Create(
            style_change_reason::kInheritedStyleChangeFromParentFrame));
  }
}

bool LocalFrame::BubbleLogicalScrollFromChildFrame(
    ScrollDirection direction,
    ScrollGranularity granularity,
    Frame* child) {
  FrameOwner* owner = child->Owner();
  auto* owner_element = DynamicTo<HTMLFrameOwnerElement>(owner);
  DCHECK(owner_element);

  return GetEventHandler().BubblingScroll(direction, granularity,
                                          owner_element);
}

LocalFrame& LocalFrame::LocalFrameRoot() const {
  const LocalFrame* cur_frame = this;
  while (cur_frame && IsA<LocalFrame>(cur_frame->Tree().Parent()))
    cur_frame = To<LocalFrame>(cur_frame->Tree().Parent());

  return const_cast<LocalFrame&>(*cur_frame);
}

bool LocalFrame::IsCrossOriginSubframe() const {
  const SecurityOrigin* security_origin =
      GetSecurityContext()->GetSecurityOrigin();
  return !security_origin->CanAccess(
      Tree().Top().GetSecurityContext()->GetSecurityOrigin());
}

scoped_refptr<InspectorTaskRunner> LocalFrame::GetInspectorTaskRunner() {
  return inspector_task_runner_;
}

void LocalFrame::StartPrinting(const FloatSize& page_size,
                               const FloatSize& original_page_size,
                               float maximum_shrink_ratio) {
  SetPrinting(true, page_size, original_page_size, maximum_shrink_ratio);
}

void LocalFrame::EndPrinting() {
  SetPrinting(false, FloatSize(), FloatSize(), 0);
}

void LocalFrame::SetPrinting(bool printing,
                             const FloatSize& page_size,
                             const FloatSize& original_page_size,
                             float maximum_shrink_ratio) {
  // In setting printing, we should not validate resources already cached for
  // the document.  See https://bugs.webkit.org/show_bug.cgi?id=43704
  ResourceCacheValidationSuppressor validation_suppressor(
      GetDocument()->Fetcher());

  GetDocument()->SetPrinting(printing ? Document::kPrinting
                                      : Document::kFinishingPrinting);
  View()->AdjustMediaTypeForPrinting(printing);

  if (TextAutosizer* text_autosizer = GetDocument()->GetTextAutosizer())
    text_autosizer->UpdatePageInfo();

  if (ShouldUsePrintingLayout()) {
    View()->ForceLayoutForPagination(page_size, original_page_size,
                                     maximum_shrink_ratio);
  } else {
    if (LayoutView* layout_view = View()->GetLayoutView()) {
      layout_view->SetPreferredLogicalWidthsDirty();
      layout_view->SetNeedsLayout(layout_invalidation_reason::kPrintingChanged);
      layout_view->SetShouldDoFullPaintInvalidationForViewAndAllDescendants();
    }
    View()->UpdateLayout();
    View()->AdjustViewSize();
  }

  // Subframes of the one we're printing don't lay out to the page size.
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child)) {
      if (printing)
        child_local_frame->StartPrinting();
      else
        child_local_frame->EndPrinting();
    }
  }

  if (auto* layout_view = View()->GetLayoutView()) {
    layout_view->AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kPrinting);
  }

  if (!printing)
    GetDocument()->SetPrinting(Document::kNotPrinting);
}

bool LocalFrame::ShouldUsePrintingLayout() const {
  if (!GetDocument()->Printing())
    return false;

  // Only the top frame being printed should be fitted to page size.
  // Subframes should be constrained by parents only.
  // This function considers the following two kinds of frames as top frames:
  // -- frame with no parent;
  // -- frame's parent is not in printing mode.
  // For the second type, it is a bit complicated when its parent is a remote
  // frame. In such case, we can not check its document or other internal
  // status. However, if the parent is in printing mode, this frame's printing
  // must have started with |use_printing_layout| as false in print context.
  auto* parent = Tree().Parent();
  if (!parent)
    return true;
  auto* local_parent = DynamicTo<LocalFrame>(parent);
  return local_parent ? !local_parent->GetDocument()->Printing()
                      : Client()->UsePrintingLayout();
}

FloatSize LocalFrame::ResizePageRectsKeepingRatio(
    const FloatSize& original_size,
    const FloatSize& expected_size) const {
  auto* layout_object = ContentLayoutObject();
  if (!layout_object)
    return FloatSize();

  bool is_horizontal = layout_object->StyleRef().IsHorizontalWritingMode();
  float width = original_size.Width();
  float height = original_size.Height();
  if (!is_horizontal)
    std::swap(width, height);
  DCHECK_GT(fabs(width), std::numeric_limits<float>::epsilon());
  float ratio = height / width;

  float result_width =
      floorf(is_horizontal ? expected_size.Width() : expected_size.Height());
  float result_height = floorf(result_width * ratio);
  if (!is_horizontal)
    std::swap(result_width, result_height);
  return FloatSize(result_width, result_height);
}

void LocalFrame::SetPageZoomFactor(float factor) {
  SetPageAndTextZoomFactors(factor, text_zoom_factor_);
}

void LocalFrame::SetTextZoomFactor(float factor) {
  SetPageAndTextZoomFactors(page_zoom_factor_, factor);
}

void LocalFrame::SetPageAndTextZoomFactors(float page_zoom_factor,
                                           float text_zoom_factor) {
  if (page_zoom_factor_ == page_zoom_factor &&
      text_zoom_factor_ == text_zoom_factor)
    return;

  Page* page = this->GetPage();
  if (!page)
    return;

  Document* document = this->GetDocument();
  if (!document)
    return;

  // Respect SVGs zoomAndPan="disabled" property in standalone SVG documents.
  // FIXME: How to handle compound documents + zoomAndPan="disabled"? Needs SVG
  // WG clarification.
  if (document->IsSVGDocument()) {
    if (!document->AccessSVGExtensions().ZoomAndPanEnabled())
      return;
  }

  page_zoom_factor_ = page_zoom_factor;
  text_zoom_factor_ = text_zoom_factor;

  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child)) {
      child_local_frame->SetPageAndTextZoomFactors(page_zoom_factor_,
                                                   text_zoom_factor_);
    }
  }

  document->MediaQueryAffectingValueChanged();
  document->SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kZoom));
  if (View() && View()->DidFirstLayout())
    document->UpdateStyleAndLayout();
}

void LocalFrame::DeviceScaleFactorChanged() {
  GetDocument()->MediaQueryAffectingValueChanged();
  GetDocument()->SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kZoom));
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child))
      child_local_frame->DeviceScaleFactorChanged();
  }
}

double LocalFrame::DevicePixelRatio() const {
  if (!page_)
    return 0;

  double ratio = page_->DeviceScaleFactorDeprecated();
  ratio *= PageZoomFactor();
  return ratio;
}

String LocalFrame::SelectedText() const {
  return Selection().SelectedText();
}

String LocalFrame::SelectedTextForClipboard() const {
  if (!GetDocument())
    return g_empty_string;
  DCHECK(!GetDocument()->NeedsLayoutTreeUpdate());
  return Selection().SelectedTextForClipboard();
}

PositionWithAffinity LocalFrame::PositionForPoint(
    const LayoutPoint& frame_point) {
  HitTestLocation location(frame_point);
  HitTestResult result = GetEventHandler().HitTestResultAtLocation(location);
  Node* node = result.InnerNodeOrImageMapImage();
  if (!node)
    return PositionWithAffinity();
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return PositionWithAffinity();
  const PositionWithAffinity position =
      layout_object->PositionForPoint(result.LocalPoint());
  if (position.IsNull())
    return PositionWithAffinity(FirstPositionInOrBeforeNode(*node));
  return position;
}

Document* LocalFrame::DocumentAtPoint(const LayoutPoint& point_in_root_frame) {
  if (!View())
    return nullptr;

  HitTestLocation location(View()->ConvertFromRootFrame(point_in_root_frame));

  if (!ContentLayoutObject())
    return nullptr;
  HitTestResult result = GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  return result.InnerNode() ? &result.InnerNode()->GetDocument() : nullptr;
}

void LocalFrame::RemoveSpellingMarkersUnderWords(const Vector<String>& words) {
  GetSpellChecker().RemoveSpellingMarkersUnderWords(words);
}

String LocalFrame::GetLayerTreeAsTextForTesting(unsigned flags) const {
  if (!ContentLayoutObject())
    return String();

  std::unique_ptr<JSONObject> layers;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    layers = View()->CompositedLayersAsJSON(static_cast<LayerTreeFlags>(flags));
  } else {
    if (const auto* root_layer =
            ContentLayoutObject()->Compositor()->RootGraphicsLayer()) {
      if (flags & kLayerTreeIncludesRootLayer && IsMainFrame()) {
        while (root_layer->Parent())
          root_layer = root_layer->Parent();
      }
      layers = GraphicsLayerTreeAsJSON(root_layer,
                                       static_cast<LayerTreeFlags>(flags));
    }
  }
  return layers ? layers->ToPrettyJSONString() : String();
}

bool LocalFrame::ShouldThrottleRendering() const {
  return View() && View()->ShouldThrottleRendering();
}

inline LocalFrame::LocalFrame(LocalFrameClient* client,
                              Page& page,
                              FrameOwner* owner,
                              InterfaceRegistry* interface_registry)
    : Frame(client,
            page,
            owner,
            MakeGarbageCollected<LocalWindowProxyManager>(*this)),
      frame_scheduler_(page.GetPageScheduler()->CreateFrameScheduler(
          this,
          client->GetFrameBlameContext(),
          IsMainFrame() ? FrameScheduler::FrameType::kMainFrame
                        : FrameScheduler::FrameType::kSubframe)),
      loader_(this),
      navigation_scheduler_(MakeGarbageCollected<NavigationScheduler>(this)),
      script_controller_(MakeGarbageCollected<ScriptController>(
          *this,
          *static_cast<LocalWindowProxyManager*>(GetWindowProxyManager()))),
      editor_(MakeGarbageCollected<Editor>(*this)),
      spell_checker_(MakeGarbageCollected<SpellChecker>(*this)),
      selection_(MakeGarbageCollected<FrameSelection>(*this)),
      event_handler_(MakeGarbageCollected<EventHandler>(*this)),
      console_(MakeGarbageCollected<FrameConsole>(*this)),
      input_method_controller_(
          MakeGarbageCollected<InputMethodController>(*this)),
      text_suggestion_controller_(
          MakeGarbageCollected<TextSuggestionController>(*this)),
      navigation_disable_count_(0),
      page_zoom_factor_(ParentPageZoomFactor(this)),
      text_zoom_factor_(ParentTextZoomFactor(this)),
      in_view_source_mode_(false),
      inspector_task_runner_(InspectorTaskRunner::Create(
          GetTaskRunner(TaskType::kInternalInspector))),
      interface_registry_(interface_registry),
      is_save_data_enabled_(
          !(GetSettings() && GetSettings()->GetDataSaverHoldbackWebApi()) &&
          GetNetworkStateNotifier().SaveDataEnabled()) {
  if (IsLocalRoot()) {
    probe_sink_ = MakeGarbageCollected<CoreProbeSink>();
    performance_monitor_ = MakeGarbageCollected<PerformanceMonitor>(this);
    inspector_trace_events_ = MakeGarbageCollected<InspectorTraceEvents>();
    probe_sink_->AddInspectorTraceEvents(inspector_trace_events_);
    if (RuntimeEnabledFeatures::AdTaggingEnabled()) {
      ad_tracker_ = MakeGarbageCollected<AdTracker>(this);
    }
  } else {
    // Inertness only needs to be updated if this frame might inherit the
    // inert state from a higher-level frame. If this is an OOPIF local root,
    // it will be updated later.
    UpdateInertIfPossible();
    UpdateInheritedEffectiveTouchActionIfPossible();
    probe_sink_ = LocalFrameRoot().probe_sink_;
    ad_tracker_ = LocalFrameRoot().ad_tracker_;
    performance_monitor_ = LocalFrameRoot().performance_monitor_;
  }
  idleness_detector_ = MakeGarbageCollected<IdlenessDetector>(this);
  inspector_task_runner_->InitIsolate(V8PerIsolateData::MainThreadIsolate());

  if (ad_tracker_) {
    SetIsAdSubframeIfNecessary();
  }
  DCHECK(ad_tracker_ ? RuntimeEnabledFeatures::AdTaggingEnabled()
                     : !RuntimeEnabledFeatures::AdTaggingEnabled());
}

FrameScheduler* LocalFrame::GetFrameScheduler() {
  return frame_scheduler_.get();
}

EventHandlerRegistry& LocalFrame::GetEventHandlerRegistry() const {
  return event_handler_->GetEventHandlerRegistry();
}

scoped_refptr<base::SingleThreadTaskRunner> LocalFrame::GetTaskRunner(
    TaskType type) {
  DCHECK(IsMainThread());
  return frame_scheduler_->GetTaskRunner(type);
}

void LocalFrame::ScheduleVisualUpdateUnlessThrottled() {
  if (ShouldThrottleRendering())
    return;
  GetPage()->Animator().ScheduleVisualUpdate(this);
}

static bool CanAccessAncestor(const SecurityOrigin& active_security_origin,
                              const Frame* target_frame) {
  // targetFrame can be 0 when we're trying to navigate a top-level frame
  // that has a 0 opener.
  if (!target_frame)
    return false;

  const bool is_local_active_origin = active_security_origin.IsLocal();
  for (const Frame* ancestor_frame = target_frame; ancestor_frame;
       ancestor_frame = ancestor_frame->Tree().Parent()) {
    const SecurityOrigin* ancestor_security_origin =
        ancestor_frame->GetSecurityContext()->GetSecurityOrigin();
    if (active_security_origin.CanAccess(ancestor_security_origin))
      return true;

    // Allow file URL descendant navigation even when
    // allowFileAccessFromFileURLs is false.
    // FIXME: It's a bit strange to special-case local origins here. Should we
    // be doing something more general instead?
    if (is_local_active_origin && ancestor_security_origin->IsLocal())
      return true;
  }

  return false;
}

bool LocalFrame::CanNavigate(const Frame& target_frame,
                             const KURL& destination_url) {
  if (&target_frame == this)
    return true;

  // Navigating window.opener cross origin, without user activation. See
  // crbug.com/813643.
  if (Client()->Opener() == target_frame &&
      !HasTransientUserActivation(this, false /* check_if_main_thread */) &&
      !target_frame.GetSecurityContext()->GetSecurityOrigin()->CanAccess(
          SecurityOrigin::Create(destination_url).get())) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kOpenerNavigationWithoutGesture);
  }

  if (GetSecurityContext()->IsSandboxed(WebSandboxFlags::kNavigation)) {
    if (!target_frame.Tree().IsDescendantOf(this) &&
        !target_frame.IsMainFrame()) {
      PrintNavigationErrorMessage(
          target_frame,
          "The frame attempting navigation is sandboxed, and is therefore "
          "disallowed from navigating its ancestors.");
      return false;
    }

    // Sandboxed frames can also navigate popups, if the
    // 'allow-sandbox-escape-via-popup' flag is specified, or if
    // 'allow-popups' flag is specified, or if the
    if (target_frame.IsMainFrame() && target_frame != Tree().Top() &&
        GetSecurityContext()->IsSandboxed(
            WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts) &&
        (GetSecurityContext()->IsSandboxed(WebSandboxFlags::kPopups) ||
         target_frame.Client()->Opener() != this)) {
      PrintNavigationErrorMessage(
          target_frame,
          "The frame attempting navigation is sandboxed and is trying "
          "to navigate a popup, but is not the popup's opener and is not "
          "set to propagate sandboxing to popups.");
      return false;
    }

    // Top navigation is forbidden unless opted-in. allow-top-navigation or
    // allow-top-navigation-by-user-activation will also skips origin checks.
    if (target_frame == Tree().Top()) {
      if (GetSecurityContext()->IsSandboxed(WebSandboxFlags::kTopNavigation) &&
          GetSecurityContext()->IsSandboxed(
              WebSandboxFlags::kTopNavigationByUserActivation)) {
        PrintNavigationErrorMessage(
            target_frame,
            "The frame attempting navigation of the top-level window is "
            "sandboxed, but the flag of 'allow-top-navigation' or "
            "'allow-top-navigation-by-user-activation' is not set.");
        return false;
      }

      if (GetSecurityContext()->IsSandboxed(WebSandboxFlags::kTopNavigation) &&
          !GetSecurityContext()->IsSandboxed(
              WebSandboxFlags::kTopNavigationByUserActivation) &&
          !LocalFrame::HasTransientUserActivation(this)) {
        // With only 'allow-top-navigation-by-user-activation' (but not
        // 'allow-top-navigation'), top navigation requires a user gesture.
        PrintNavigationErrorMessage(
            target_frame,
            "The frame attempting navigation of the top-level window is "
            "sandboxed with the 'allow-top-navigation-by-user-activation' "
            "flag, but has no user activation (aka gesture). See "
            "https://www.chromestatus.com/feature/5629582019395584.");
        return false;
      }
      return true;
    }
  }

  DCHECK(GetSecurityContext()->GetSecurityOrigin());
  const SecurityOrigin& origin = *GetSecurityContext()->GetSecurityOrigin();

  // This is the normal case. A document can navigate its decendant frames,
  // or, more generally, a document can navigate a frame if the document is
  // in the same origin as any of that frame's ancestors (in the frame
  // hierarchy).
  //
  // See http://www.adambarth.com/papers/2008/barth-jackson-mitchell.pdf for
  // historical information about this security check.
  if (CanAccessAncestor(origin, &target_frame))
    return true;

  // Top-level frames are easier to navigate than other frames because they
  // display their URLs in the address bar (in most browsers). However, there
  // are still some restrictions on navigation to avoid nuisance attacks.
  // Specifically, a document can navigate a top-level frame if that frame
  // opened the document or if the document is the same-origin with any of
  // the top-level frame's opener's ancestors (in the frame hierarchy).
  //
  // In both of these cases, the document performing the navigation is in
  // some way related to the frame being navigate (e.g., by the "opener"
  // and/or "parent" relation). Requiring some sort of relation prevents a
  // document from navigating arbitrary, unrelated top-level frames.
  if (!target_frame.Tree().Parent()) {
    if (target_frame == Client()->Opener())
      return true;
    if (CanAccessAncestor(origin, target_frame.Client()->Opener()))
      return true;
  }

  if (target_frame == Tree().Top()) {
    // A frame navigating its top may blocked if the document initiating
    // the navigation has never received a user gesture and the navigation
    // isn't same-origin with the target.
    if (HasBeenActivated() ||
        target_frame.GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            SecurityOrigin::Create(destination_url).get())) {
      return true;
    }

    String target_domain = network_utils::GetDomainAndRegistry(
        target_frame.GetSecurityContext()->GetSecurityOrigin()->Domain(),
        network_utils::kIncludePrivateRegistries);
    String destination_domain = network_utils::GetDomainAndRegistry(
        destination_url.Host(), network_utils::kIncludePrivateRegistries);
    if (!target_domain.IsEmpty() && !destination_domain.IsEmpty() &&
        target_domain == destination_domain) {
      return true;
    }
    if (auto* settings_client = Client()->GetContentSettingsClient()) {
      if (settings_client->AllowPopupsAndRedirects(false /* default_value*/))
        return true;
    }
    PrintNavigationErrorMessage(
        target_frame,
        "The frame attempting navigation is targeting its top-level window, "
        "but is neither same-origin with its target nor has it received a "
        "user gesture. See "
        "https://www.chromestatus.com/features/5851021045661696.");
    Client()->DidBlockFramebust(destination_url);
  } else {
    PrintNavigationErrorMessage(target_frame,
                                "The frame attempting navigation is neither "
                                "same-origin with the target, "
                                "nor is it the target's parent or opener.");
  }
  return false;
}

void LocalFrame::SetIsAdSubframeIfNecessary() {
  DCHECK(ad_tracker_);
  Frame* parent = Tree().Parent();
  if (!parent)
    return;

  // If the parent frame is local, directly determine if it's an ad. If it's
  // remote, then it is up to the embedder that moved this frame out-of-
  // process to set this frame as an ad via SetIsAdSubframe before commit.
  auto* parent_local_frame = DynamicTo<LocalFrame>(parent);
  bool parent_is_ad = parent_local_frame && parent_local_frame->IsAdSubframe();

  if (parent_is_ad || ad_tracker_->IsAdScriptInStack()) {
    SetIsAdSubframe(parent_is_ad ? blink::mojom::AdFrameType::kChildAd
                                 : blink::mojom::AdFrameType::kRootAd);
  }
}

ContentCaptureManager* LocalFrame::GetContentCaptureManager() {
  DCHECK(Client());
  if (!IsLocalRoot())
    return nullptr;

  if (auto* content_capture_client = Client()->GetWebContentCaptureClient()) {
    if (!content_capture_manager_) {
      content_capture_manager_ = MakeGarbageCollected<ContentCaptureManager>(
          *this, content_capture_client->GetNodeHolderType());
    }
  } else if (content_capture_manager_) {
    content_capture_manager_->Shutdown();
    content_capture_manager_ = nullptr;
  }
  return content_capture_manager_;
}

service_manager::InterfaceProvider& LocalFrame::GetInterfaceProvider() {
  DCHECK(Client());
  return *Client()->GetInterfaceProvider();
}

void LocalFrame::BindDocumentInterfaceBroker(
    mojo::ScopedMessagePipeHandle js_handle) {
  DCHECK(Client());
  Client()->BindDocumentInterfaceBroker(std::move(js_handle));
}

mojom::blink::DocumentInterfaceBroker&
LocalFrame::GetDocumentInterfaceBroker() {
  DCHECK(Client());
  return *Client()->GetDocumentInterfaceBroker();
}

mojo::ScopedMessagePipeHandle LocalFrame::SetDocumentInterfaceBrokerForTesting(
    mojo::ScopedMessagePipeHandle blink_handle) {
  DCHECK(Client());
  return Client()->SetDocumentInterfaceBrokerForTesting(
      std::move(blink_handle));
}

AssociatedInterfaceProvider*
LocalFrame::GetRemoteNavigationAssociatedInterfaces() {
  DCHECK(Client());
  return Client()->GetRemoteNavigationAssociatedInterfaces();
}

LocalFrameClient* LocalFrame::Client() const {
  return static_cast<LocalFrameClient*>(Frame::Client());
}

WebContentSettingsClient* LocalFrame::GetContentSettingsClient() {
  return Client() ? Client()->GetContentSettingsClient() : nullptr;
}

FrameResourceCoordinator* LocalFrame::GetFrameResourceCoordinator() {
  if (!frame_resource_coordinator_) {
    auto* local_frame_client = Client();
    if (!local_frame_client)
      return nullptr;
    frame_resource_coordinator_ = FrameResourceCoordinator::MaybeCreate(
        local_frame_client->GetInterfaceProvider());
  }
  return frame_resource_coordinator_.get();
}

PluginData* LocalFrame::GetPluginData() const {
  if (!Loader().AllowPlugins(kNotAboutToInstantiatePlugin))
    return nullptr;
  return GetPage()->GetPluginData(
      Tree().Top().GetSecurityContext()->GetSecurityOrigin());
}

void LocalFrame::SetAdTrackerForTesting(AdTracker* ad_tracker) {
  if (ad_tracker_)
    ad_tracker_->Shutdown();
  ad_tracker_ = ad_tracker;
}

DEFINE_WEAK_IDENTIFIER_MAP(LocalFrame)

FrameNavigationDisabler::FrameNavigationDisabler(LocalFrame& frame)
    : frame_(&frame) {
  frame_->DisableNavigation();
}

FrameNavigationDisabler::~FrameNavigationDisabler() {
  frame_->EnableNavigation();
}

namespace {

bool IsScopedFrameBlamerEnabled() {
  // Must match the category used in content::FrameBlameContext.
  static const auto* enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("blink");
  return *enabled;
}

}  // namespace

ScopedFrameBlamer::ScopedFrameBlamer(LocalFrame* frame)
    : frame_(IsScopedFrameBlamerEnabled() ? frame : nullptr) {
  if (LIKELY(!frame_))
    return;
  LocalFrameClient* client = frame_->Client();
  if (!client)
    return;
  if (BlameContext* context = client->GetFrameBlameContext())
    context->Enter();
}

void ScopedFrameBlamer::LeaveContext() {
  LocalFrameClient* client = frame_->Client();
  if (!client)
    return;
  if (BlameContext* context = client->GetFrameBlameContext())
    context->Leave();
}

bool LocalFrame::IsClientLoFiAllowed(const ResourceRequest& request) const {
  return Client() && ShouldUseClientLoFiForRequest(
                         request, Client()->GetPreviewsStateForFrame());
}

bool LocalFrame::IsLazyLoadingImageAllowed() const {
  if (!RuntimeEnabledFeatures::LazyImageLoadingEnabled())
    return false;
  if (Owner() && !Owner()->ShouldLazyLoadChildren())
    return false;
  if (RuntimeEnabledFeatures::RestrictLazyImageLoadingToDataSaverEnabled() &&
      !is_save_data_enabled_) {
    return false;
  }
  return true;
}

WebURLLoaderFactory* LocalFrame::GetURLLoaderFactory() {
  if (!url_loader_factory_)
    url_loader_factory_ = Client()->CreateURLLoaderFactory();
  return url_loader_factory_.get();
}

WebPluginContainerImpl* LocalFrame::GetWebPluginContainer(Node* node) const {
  if (GetDocument() && GetDocument()->IsPluginDocument()) {
    return ToPluginDocument(GetDocument())->GetPluginView();
  }
  if (!node) {
    DCHECK(GetDocument());
    node = GetDocument()->FocusedElement();
  }

  if (node) {
    return node->GetWebPluginContainer();
  }
  return nullptr;
}

void LocalFrame::SetViewportIntersectionFromParent(
    const IntRect& viewport_intersection,
    FrameOcclusionState occlusion_state) {
  if (remote_viewport_intersection_ != viewport_intersection ||
      occlusion_state_ != occlusion_state) {
    remote_viewport_intersection_ = viewport_intersection;
    occlusion_state_ = occlusion_state;
    if (View()) {
      View()->SetIntersectionObservationState(LocalFrameView::kRequired);
      View()->ScheduleAnimation();
    }
  }
}

FrameOcclusionState LocalFrame::GetOcclusionState() const {
  // TODO(dcheng): Get rid of this branch for the main frame.
  if (IsMainFrame())
    return FrameOcclusionState::kGuaranteedNotOccluded;
  if (IsLocalRoot())
    return occlusion_state_;
  return LocalFrameRoot().GetOcclusionState();
}

void LocalFrame::ForceSynchronousDocumentInstall(
    const AtomicString& mime_type,
    scoped_refptr<SharedBuffer> data) {
  CHECK(loader_.StateMachine()->IsDisplayingInitialEmptyDocument());
  DCHECK(!Client()->IsLocalFrameClientImpl());

  // Any Document requires Shutdown() before detach, even the initial empty
  // document.
  GetDocument()->Shutdown();

  DomWindow()->InstallNewDocument(
      mime_type,
      DocumentInit::Create().WithDocumentLoader(loader_.GetDocumentLoader()),
      false);
  loader_.StateMachine()->AdvanceTo(
      FrameLoaderStateMachine::kCommittedFirstRealLoad);

  GetDocument()->OpenForNavigation(kForceSynchronousParsing, mime_type,
                                   AtomicString("UTF-8"));
  for (const auto& segment : *data)
    GetDocument()->Parser()->AppendBytes(segment.data(), segment.size());
  GetDocument()->Parser()->Finish();

  // Upon loading of SVGIamges, log PageVisits in UseCounter.
  // Do not track PageVisits for inspector, web page popups, and validation
  // message overlays (the other callers of this method).
  if (GetDocument()->IsSVGDocument())
    loader_.GetDocumentLoader()->GetUseCounter().DidCommitLoad(this);
}

bool LocalFrame::IsProvisional() const {
  // Calling this after the frame is marked as completely detached is a bug, as
  // this state can no longer be accurately calculated.
  CHECK(!IsDetached());

  if (IsMainFrame()) {
    return GetPage()->MainFrame() != this;
  }

  DCHECK(Owner());
  return Owner()->ContentFrame() != this;
}

bool LocalFrame::IsUsingDataSavingPreview() const {
  if (!Client())
    return false;

  WebURLRequest::PreviewsState previews_state =
      Client()->GetPreviewsStateForFrame();
  // Check for any data saving type of preview.
  return previews_state &
         (WebURLRequest::kServerLoFiOn | WebURLRequest::kClientLoFiOn |
          WebURLRequest::kNoScriptOn);
}

bool LocalFrame::IsAdSubframe() const {
  return ad_frame_type_ != blink::mojom::AdFrameType::kNonAd;
}

bool LocalFrame::IsAdRoot() const {
  return ad_frame_type_ == blink::mojom::AdFrameType::kRootAd;
}

void LocalFrame::SetIsAdSubframe(blink::mojom::AdFrameType ad_frame_type) {
  DCHECK(!IsMainFrame());

  // Once |ad_frame_type_| has been set to an ad type on this frame, it cannot
  // be changed.
  if (ad_frame_type == blink::mojom::AdFrameType::kNonAd)
    return;
  if (ad_frame_type_ != blink::mojom::AdFrameType::kNonAd)
    return;
  ad_frame_type_ = ad_frame_type;
  UpdateAdHighlight();
  frame_scheduler_->SetIsAdFrame();
  InstanceCounters::IncrementCounter(InstanceCounters::kAdSubframeCounter);
}

void LocalFrame::UpdateAdHighlight() {
  if (!IsAdRoot()) {
    // Verify that non root ad subframes do not have an overlay.
    DCHECK(IsMainFrame() || !frame_color_overlay_);
    return;
  }
  if (GetPage()->GetSettings().GetHighlightAds())
    SetSubframeColorOverlay(SkColorSetARGB(128, 255, 0, 0));
  else
    SetSubframeColorOverlay(Color::kTransparent);
}

void LocalFrame::PauseSubresourceLoading(
    blink::mojom::blink::PauseSubresourceLoadingHandleRequest request) {
  auto handle = GetFrameScheduler()->GetPauseSubresourceLoadingHandle();
  if (!handle)
    return;
  pause_handle_bindings_.AddBinding(std::move(handle), std::move(request));
}

void LocalFrame::ResumeSubresourceLoading() {
  pause_handle_bindings_.CloseAllBindings();
}

void LocalFrame::AnimateSnapFling(base::TimeTicks monotonic_time) {
  GetEventHandler().AnimateSnapFling(monotonic_time);
}

void LocalFrame::BindPreviewsResourceLoadingHintsRequest(
    blink::mojom::blink::PreviewsResourceLoadingHintsReceiverRequest request) {
  DCHECK(!previews_resource_loading_hints_receiver_);
  previews_resource_loading_hints_receiver_ =
      std::make_unique<PreviewsResourceLoadingHintsReceiverImpl>(
          std::move(request), GetDocument());
}

SmoothScrollSequencer& LocalFrame::GetSmoothScrollSequencer() {
  if (!IsLocalRoot())
    return LocalFrameRoot().GetSmoothScrollSequencer();
  if (!smooth_scroll_sequencer_)
    smooth_scroll_sequencer_ = MakeGarbageCollected<SmoothScrollSequencer>();
  return *smooth_scroll_sequencer_;
}

ukm::UkmRecorder* LocalFrame::GetUkmRecorder() {
  Document* document = GetDocument();
  if (!document)
    return nullptr;
  return document->UkmRecorder();
}

int64_t LocalFrame::GetUkmSourceId() {
  Document* document = GetDocument();
  if (!document)
    return ukm::kInvalidSourceId;
  return document->UkmSourceID();
}

void LocalFrame::UpdateTaskTime(base::TimeDelta time) {
  Client()->DidChangeCpuTiming(time);
}

void LocalFrame::UpdateActiveSchedulerTrackedFeatures(uint64_t features_mask) {
  Client()->DidChangeActiveSchedulerTrackedFeatures(features_mask);
}

const mojom::blink::ReportingServiceProxyPtr& LocalFrame::GetReportingService()
    const {
  if (!reporting_service_) {
    Platform::Current()->GetConnector()->BindInterface(
        Platform::Current()->GetBrowserServiceName(), &reporting_service_);
  }
  return reporting_service_;
}

// static
std::unique_ptr<UserGestureIndicator> LocalFrame::NotifyUserActivation(
    LocalFrame* frame,
    UserGestureToken::Status status) {
  if (frame) {
    UserActivationNotifierFrame() = frame;
    frame->NotifyUserActivation();
  }
  return std::make_unique<UserGestureIndicator>(status);
}

// static
std::unique_ptr<UserGestureIndicator> LocalFrame::NotifyUserActivation(
    LocalFrame* frame,
    UserGestureToken* token) {
  DCHECK(!RuntimeEnabledFeatures::UserActivationV2Enabled());
  if (frame) {
    UserActivationNotifierFrame() = frame;
    frame->NotifyUserActivation();
  }
  return std::make_unique<UserGestureIndicator>(token);
}

// static
bool LocalFrame::HasTransientUserActivation(LocalFrame* frame,
                                            bool check_if_main_thread) {
  bool available;

  if (RuntimeEnabledFeatures::UserActivationV2Enabled()) {
    available = frame ? frame->HasTransientUserActivation() : false;
  } else {
    available = check_if_main_thread
                    ? UserGestureIndicator::ProcessingUserGestureThreadSafe()
                    : UserGestureIndicator::ProcessingUserGesture();
  }

  const bool off_main_thread = check_if_main_thread && !IsMainThread();
  UMA_HISTOGRAM_ENUMERATION(
      "UserActivation.AvailabilityCheck.FrameResult",
      DetermineActivationResultEnum(frame, available, off_main_thread));

  return available;
}

// static
bool LocalFrame::ConsumeTransientUserActivation(
    LocalFrame* frame,
    bool check_if_main_thread,
    UserActivationUpdateSource update_source) {
  bool consumed;

  if (RuntimeEnabledFeatures::UserActivationV2Enabled()) {
    consumed =
        frame ? frame->ConsumeTransientUserActivation(update_source) : false;
  } else {
    consumed = check_if_main_thread
                   ? UserGestureIndicator::ConsumeUserGestureThreadSafe()
                   : UserGestureIndicator::ConsumeUserGesture();
  }

  const bool off_main_thread = check_if_main_thread && !IsMainThread();
  UMA_HISTOGRAM_ENUMERATION(
      "UserActivation.Consumption.FrameResult",
      DetermineActivationResultEnum(frame, consumed, off_main_thread));
  if (!off_main_thread)
    UserActivationNotifierFrame().Clear();

  return consumed;
}

void LocalFrame::NotifyUserActivation() {
  Client()->NotifyUserActivation();
  NotifyUserActivationInLocalTree();
}

bool LocalFrame::HasTransientUserActivation() {
  return user_activation_state_.IsActive();
}

bool LocalFrame::ConsumeTransientUserActivation(
    UserActivationUpdateSource update_source) {
  if (update_source == UserActivationUpdateSource::kRenderer)
    Client()->ConsumeUserActivation();
  return ConsumeTransientUserActivationInLocalTree();
}

namespace {

class FrameColorOverlay final : public FrameOverlay::Delegate {
 public:
  explicit FrameColorOverlay(LocalFrame* frame, SkColor color)
      : color_(color), frame_(frame) {}

 private:
  void PaintFrameOverlay(const FrameOverlay& frame_overlay,
                         GraphicsContext& graphics_context,
                         const IntSize&) const override {
    const auto* view = frame_->View();
    DCHECK(view);
    if (view->Width() == 0 || view->Height() == 0)
      return;
    ScopedPaintChunkProperties properties(
        graphics_context.GetPaintController(),
        view->GetLayoutView()->FirstFragment().LocalBorderBoxProperties(),
        frame_overlay, DisplayItem::kFrameOverlay);
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            graphics_context, frame_overlay, DisplayItem::kFrameOverlay))
      return;
    DrawingRecorder recorder(graphics_context, frame_overlay,
                             DisplayItem::kFrameOverlay);
    FloatRect rect(0, 0, view->Width(), view->Height());
    graphics_context.FillRect(rect, color_);
  }

  SkColor color_;
  Persistent<LocalFrame> frame_;
};

}  // namespace

void LocalFrame::SetMainFrameColorOverlay(SkColor color) {
  DCHECK(IsMainFrame());
  SetFrameColorOverlay(color);
}

void LocalFrame::SetSubframeColorOverlay(SkColor color) {
  DCHECK(!IsMainFrame());
  SetFrameColorOverlay(color);
}

void LocalFrame::SetFrameColorOverlay(SkColor color) {
  frame_color_overlay_.reset();

  if (color == Color::kTransparent)
    return;

  frame_color_overlay_ = std::make_unique<FrameOverlay>(
      this, std::make_unique<FrameColorOverlay>(this, color));
}

void LocalFrame::UpdateFrameColorOverlayPrePaint() {
  if (frame_color_overlay_)
    frame_color_overlay_->UpdatePrePaint();
}

void LocalFrame::PaintFrameColorOverlay(GraphicsContext& context) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  if (frame_color_overlay_)
    frame_color_overlay_->Paint(context);
}

void LocalFrame::ForciblyPurgeV8Memory() {
  GetDocument()->NotifyContextDestroyed();

  WindowProxyManager* window_proxy_manager = GetWindowProxyManager();
  window_proxy_manager->ClearForV8MemoryPurge();
  Loader().StopAllLoaders();
}

void LocalFrame::PauseContext() {
  if (Document* document = GetDocument()) {
    document->Fetcher()->SetDefersLoading(true);
    document->SetLifecycleState(lifecycle_state_);
  }
  Loader().SetDefersLoading(true);
  GetFrameScheduler()->SetPaused(true);
}

void LocalFrame::UnpauseContext() {
  if (Document* document = GetDocument()) {
    document->Fetcher()->SetDefersLoading(false);
    document->SetLifecycleState(mojom::FrameLifecycleState::kRunning);
  }
  Loader().SetDefersLoading(false);
  GetFrameScheduler()->SetPaused(false);
}

void LocalFrame::SetLifecycleState(mojom::FrameLifecycleState state) {
  if (state == lifecycle_state_)
    return;
  bool is_frozen = lifecycle_state_ != mojom::FrameLifecycleState::kRunning;
  bool freeze = state != mojom::FrameLifecycleState::kRunning;

  // TODO(dtapuska): Determine if we should dispatch events if we are
  // transitioning across frozen states. ie. kPaused->kFrozen should
  // pause media.

  // If we are transitioning from one frozen state to another just return.
  if (is_frozen == freeze)
    return;
  mojom::FrameLifecycleState old_state = lifecycle_state_;
  lifecycle_state_ = state;

  if (freeze) {
    if (lifecycle_state_ != mojom::FrameLifecycleState::kPaused)
      DidFreeze();
    PauseContext();
  } else {
    UnpauseContext();
    if (old_state != mojom::FrameLifecycleState::kPaused)
      DidResume();
  }
}

void LocalFrame::MaybeLogAdClickNavigation() {
  if (HasTransientUserActivation() && IsAdSubframe())
    UseCounter::Count(GetDocument(), WebFeature::kAdClickNavigation);
}

}  // namespace blink
