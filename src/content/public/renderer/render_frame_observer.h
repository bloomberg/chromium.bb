// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_

#include <stdint.h>

#include <string>

#include "base/memory/read_only_shared_memory_region.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/public/common/use_counter/use_counter_feature.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/page_transition_types.h"
#include "v8/include/v8-forward.h"

class GURL;

namespace blink {
class WebDocumentLoader;
class WebElement;
class WebFormElement;
class WebString;
class WebWorkerFetchContext;
}  // namespace blink

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace gfx {
class Rect;
}  // namespace gfx

namespace content {

class RendererPpapiHost;
class RenderFrame;
class RenderFrameImpl;

// Base class for objects that want to filter incoming IPCs, and also get
// notified of changes to the frame.
class CONTENT_EXPORT RenderFrameObserver : public IPC::Listener,
                                           public IPC::Sender {
 public:
  RenderFrameObserver(const RenderFrameObserver&) = delete;
  RenderFrameObserver& operator=(const RenderFrameObserver&) = delete;

  // A subclass can use this to delete itself. If it does not, the subclass must
  // always null-check each call to render_frame() because the RenderFrame can
  // go away at any time.
  virtual void OnDestruct() = 0;

  // Called when a Pepper plugin is created.
  virtual void DidCreatePepperPlugin(RendererPpapiHost* host) {}

  // Called when a load is explicitly stopped by the user or browser.
  virtual void OnStop() {}

  // Called when the RenderFrame visiblity is changed.
  virtual void WasHidden() {}
  virtual void WasShown() {}

  // Navigation callbacks.
  //
  // Each navigation starts with a DidStartNavigation call. Then it may be
  // followed by a ReadyToCommitNavigation (if the navigation has succeeded),
  // and should always end with a DidFinishNavigation.
  // TODO(dgozman): ReadyToCommitNavigation will be removed soon.
  //
  // Unfortunately, this is currently a mess. For example, some started
  // navigations which did not commit won't receive any further notifications.
  // DidCommitProvisionalLoad will be called for same-document navigations,
  // without any other notifications. DidFailProvisionalLoad will be called
  // when committing error pages, in addition to all the methods (start, ready,
  // commit) for the error page load itself.

  // Called when the RenderFrame has started a navigation.
  // |url| is a url being navigated to. Note that final url might be different
  // due to redirects.
  // |navigation_type| is only present for renderer-initiated navigations, e.g.
  // JavaScript call, link click, form submit. User-initiated navigations from
  // the browser process (e.g. by typing a url) won't have a navigation type.
  virtual void DidStartNavigation(
      const GURL& url,
      absl::optional<blink::WebNavigationType> navigation_type) {}

  // Called when a navigation has just committed and |document_loader|
  // will start loading a new document in the RenderFrame.
  // TODO(dgozman): the name does not match functionality anymore, we should
  // merge this with DidCommitProvisionalLoad, which will become
  // DidFinishNavigation.
  virtual void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) {}

  // Called when a RenderFrame's page lifecycle state gets updated.
  virtual void DidSetPageLifecycleState() {}

  // These match the Blink API notifications. These will not be called for the
  // initial empty document, since that already exists before an observer for a
  // frame has a chance to be created (before notification about the RenderFrame
  // being created occurs).
  virtual void DidCreateNewDocument() {}
  virtual void DidCreateDocumentElement() {}
  // TODO(dgozman): replace next two methods with DidFinishNavigation.
  // DidCommitProvisionalLoad is only called for new-document navigations.
  // Use DidFinishSameDocumentNavigation for same-document navigations.
  virtual void DidCommitProvisionalLoad(ui::PageTransition transition) {}
  virtual void DidFailProvisionalLoad() {}
  virtual void DidFinishLoad() {}
  virtual void DidDispatchDOMContentLoadedEvent() {}
  virtual void DidHandleOnloadEvents() {}
  virtual void DidCreateScriptContext(v8::Local<v8::Context> context,
                                      int32_t world_id) {}
  virtual void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                        int32_t world_id) {}
  virtual void DidClearWindowObject() {}
  virtual void DidChangeScrollOffset() {}
  virtual void WillSendSubmitEvent(const blink::WebFormElement& form) {}
  virtual void WillSubmitForm(const blink::WebFormElement& form) {}
  virtual void DidMatchCSS(
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors) {}

  // Called when same-document navigation finishes.
  // This is the only callback for same-document navigations,
  // DidStartNavigation and ReadyToCommitNavigation are not called.
  //
  // Same-document navigation is typically initiated by an anchor click
  // (that usually results in the page scrolling to the anchor) or a
  // history web API manipulation.
  //
  // However, it could be some rare case like user changing #hash in the url
  // bar or history restore for subframe or anything else that was classified
  // as same-document.
  virtual void DidFinishSameDocumentNavigation() {}

  // Called when this frame has been detached from the view. This *will* be
  // called for child frames when a parent frame is detached. Since the frame is
  // already detached from the DOM at this time, it should not be inspected.
  virtual void WillDetach() {}

  // Called when we receive a console message from Blink for which we requested
  // extra details (like the stack trace). |message| is the error message,
  // |source| is the Blink-reported source of the error (either external or
  // internal), and |stack_trace| is the stack trace of the error in a
  // human-readable format (each frame is formatted as
  // "\n    at function_name (source:line_number:column_number)").
  virtual void DetailedConsoleMessageAdded(const std::u16string& message,
                                           const std::u16string& source,
                                           const std::u16string& stack_trace,
                                           uint32_t line_number,
                                           int32_t severity_level) {}

  // Called when an interesting (from document lifecycle perspective),
  // compositor-driven layout had happened. This is a reasonable hook to use
  // to inspect the document and layout information, since it is in a clean
  // state and you won't accidentally force new layouts.
  // The interestingness of layouts is explained in WebMeaningfulLayout.h.
  virtual void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) {}

  // Notifications when |PerformanceTiming| data becomes available
  virtual void DidChangePerformanceTiming() {}

  // Notifications When an input delay data becomes available.
  virtual void DidObserveInputDelay(base::TimeDelta input_delay) {}

  // Notifications When a user interaction latency data becomes available.
  virtual void DidObserveUserInteraction(
      base::TimeDelta max_event_duration,
      base::TimeDelta total_event_duration,
      blink::UserInteractionType interaction_type) {}

  // Notification When the First Scroll Delay becomes available.
  virtual void DidObserveFirstScrollDelay(base::TimeDelta first_scroll_delay) {}

  // Notifications when a cpu timing update becomes available, when a frame
  // has performed at least 100ms of tasks.
  virtual void DidChangeCpuTiming(base::TimeDelta time) {}

  // Notification when the renderer uses a particular code path during a page
  // load. This is used for metrics collection.
  virtual void DidObserveLoadingBehavior(blink::LoadingBehaviorFlag behavior) {}

  // Notification when the renderer observes a new use counter usage during a
  // page load. This is used for UseCounter metrics.
  virtual void DidObserveNewFeatureUsage(
      const blink::UseCounterFeature& feature) {}

  // Reports that visible elements in the frame shifted (bit.ly/lsm-explainer).
  // This is called once for each animation frame containing any layout shift,
  // and receives the layout shift (LS) score for that frame.  The cumulative
  // layout shift (CLS) score can be inferred by summing the LS scores.
  // |after_input_or_scroll| indicates whether the given |score| was observed
  // after an input or scroll occurred in the associated document.
  virtual void DidObserveLayoutShift(double score, bool after_input_or_scroll) {
  }

  // Reports the number of LayoutBlock creation, and LayoutObject::UpdateLayout
  // calls. All values are deltas since the last calls of this function.
  virtual void DidObserveLayoutNg(uint32_t all_block_count,
                                  uint32_t ng_block_count,
                                  uint32_t all_call_count,
                                  uint32_t ng_call_count,
                                  uint32_t flexbox_ng_block_count,
                                  uint32_t grid_ng_block_count) {}

  // Reports lazy loaded behavior when the frame or image is fully deferred or
  // if the frame or image is loaded after being deferred by lazy load.
  // Called every time the behavior occurs. This does not apply to image
  // requests for placeholder images.
  virtual void DidObserveLazyLoadBehavior(
      blink::WebLocalFrameClient::LazyLoadBehavior lazy_load_behavior) {}

#if !defined(OS_ANDROID)
  // Reports that a resource will be requested.
  virtual void WillSendRequest(const blink::WebURLRequest& request) {}
#endif

  // Notification when the renderer a response started, completed or canceled.
  // Complete or Cancel is guaranteed to be called for a response that started.
  // |request_id| uniquely identifies the request within this render frame.
  // |previews_state| is the PreviewsState if the request is a sub-resource. For
  // Document resources, |previews_state| should be reported as PREVIEWS_OFF.
  virtual void DidStartResponse(
      const GURL& response_url,
      int request_id,
      const network::mojom::URLResponseHead& response_head,
      network::mojom::RequestDestination request_destination,
      blink::PreviewsState previews_state) {}
  virtual void DidCompleteResponse(
      int request_id,
      const network::URLLoaderCompletionStatus& status) {}
  virtual void DidCancelResponse(int request_id) {}

  // Reports that a resource was loaded from the blink memory cache.
  // |request_id| uniquely identifies this resource within this render frame.
  // |from_archive| indicates if the resource originated from a MHTML archive.
  virtual void DidLoadResourceFromMemoryCache(const GURL& response_url,
                                              int request_id,
                                              int64_t encoded_body_length,
                                              const std::string& mime_type,
                                              bool from_archive) {}

  // Notification when the renderer observes data used during the page load.
  // This is used for page load metrics. |received_data_length| is the received
  // network bytes. |resource_id| uniquely identifies the resource within this
  // render frame.
  virtual void DidReceiveTransferSizeUpdate(int resource_id,
                                            int received_data_length) {}

  // Called when the focused element has changed to |element|.
  virtual void FocusedElementChanged(const blink::WebElement& element) {}

  // Called when accessibility is enabled or disabled.
  virtual void AccessibilityModeChanged(const ui::AXMode& mode) {}

  // Called when script in the page calls window.print().
  virtual void ScriptedPrint(bool user_initiated) {}

  // Called when draggable regions change.
  virtual void DraggableRegionsChanged() {}

  // Called when a worker fetch context will be created.
  virtual void WillCreateWorkerFetchContext(blink::WebWorkerFetchContext*) {}

  // Called when a frame's intersection with the main frame changes.
  virtual void OnMainFrameIntersectionChanged(const gfx::Rect& intersect_rect) {
  }

  // Overlay-popup-ad violates The Better Ads Standards
  // (https://www.betterads.org/standards/). This method will be called when an
  // overlay-popup-ad is detected, to let the embedder
  // (i.e. subresource_filter::ContentSubresourceFilterThrottleManager) know the
  // violation so as to apply further interventions.
  virtual void OnOverlayPopupAdDetected() {}

  // Large-sticky-ad violates The Better Ads Standards
  // (https://www.betterads.org/standards/). This method will be called when a
  // large-sticky-ad is detected, to let the embedder
  // (i.e. subresource_filter::ContentSubresourceFilterThrottleManager) know the
  // violation so as to apply further interventions.
  virtual void OnLargeStickyAdDetected() {}

  // Called to give the embedder an opportunity to bind an interface request
  // for a frame. If the request can be bound, |interface_pipe| will be taken.
  virtual void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) {}

  // Similar to above but for handling Channel-associated interface requests.
  // Returns |true| if the request is handled by the implementation (taking
  // ownership of |*handle|) and |false| otherwise (leaving |*handle|
  // unmodified).
  virtual bool OnAssociatedInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle);

  // Called when a page's mobile friendliness changed.
  virtual void OnMobileFriendlinessChanged(const blink::MobileFriendliness&) {}

  // The smoothness metrics is shared over shared-memory. The interested
  // observer should invalidate |shared_memory| (by std::move()'ing it), and
  // return true. All other observers should return false (default).
  virtual bool SetUpSmoothnessReporting(
      base::ReadOnlySharedMemoryRegion& shared_memory);

  // Notifies the observers of the origins for which subresource redirect
  // optimizations can be preloaded.
  virtual void PreloadSubresourceOptimizationsForOrigins(
      const std::vector<blink::WebSecurityOrigin>& origins) {}

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation.
  bool Send(IPC::Message* message) override;

  RenderFrame* render_frame() const;
  int routing_id() const { return routing_id_; }

 protected:
  explicit RenderFrameObserver(RenderFrame* render_frame);
  ~RenderFrameObserver() override;

 private:
  friend class RenderFrameImpl;

  // This is called by the RenderFrame when it's going away so that this object
  // can null out its pointer.
  void RenderFrameGone();

  RenderFrame* render_frame_;
  // The routing ID of the associated RenderFrame.
  int routing_id_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_
