// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_TARGETER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_TARGETER_H_

#include <queue>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_export.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/latency/latency_info.h"

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace gfx {
class PointF;
}

namespace content {

class RenderWidgetHostViewBase;
class OneShotTimeoutMonitor;

// TODO(sunxd): Make |RenderWidgetTargetResult| a class. Merge the booleans into
// a mask to reduce the size. Make the constructor take in enums for better
// readability.
struct CONTENT_EXPORT RenderWidgetTargetResult {
  RenderWidgetTargetResult();
  RenderWidgetTargetResult(const RenderWidgetTargetResult&);
  RenderWidgetTargetResult(RenderWidgetHostViewBase* view,
                           bool should_query_view,
                           base::Optional<gfx::PointF> location,
                           bool latched_target,
                           bool should_verify_result);
  ~RenderWidgetTargetResult();

  RenderWidgetHostViewBase* view = nullptr;
  bool should_query_view = false;
  base::Optional<gfx::PointF> target_location = base::nullopt;
  // When |latched_target| is false, we explicitly hit-tested events instead of
  // using a known target.
  bool latched_target = false;
  // When |should_verify_result| is true, RenderWidgetTargeter will do async hit
  // testing and compare the target with the result of synchronous hit testing.
  // |should_verify_result| will always be false if we are doing draw quad based
  // hit testing.
  bool should_verify_result = false;
};

class TracingUmaTracker;

class RenderWidgetTargeter {
 public:
  using RenderWidgetHostAtPointCallback =
      base::OnceCallback<void(base::WeakPtr<RenderWidgetHostViewBase>,
                              base::Optional<gfx::PointF>)>;

  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual RenderWidgetTargetResult FindTargetSynchronouslyAtPoint(
        RenderWidgetHostViewBase* root_view,
        const gfx::PointF& location) = 0;

    virtual RenderWidgetTargetResult FindTargetSynchronously(
        RenderWidgetHostViewBase* root_view,
        const blink::WebInputEvent& event) = 0;

    // |event| is in |root_view|'s coordinate space.
    virtual void DispatchEventToTarget(
        RenderWidgetHostViewBase* root_view,
        RenderWidgetHostViewBase* target,
        const blink::WebInputEvent& event,
        const ui::LatencyInfo& latency,
        const base::Optional<gfx::PointF>& target_location) = 0;

    virtual void SetEventsBeingFlushed(bool events_being_flushed) = 0;

    virtual RenderWidgetHostViewBase* FindViewFromFrameSinkId(
        const viz::FrameSinkId& frame_sink_id) const = 0;

    // Returns true if a further asynchronous query should be sent to the
    // candidate RenderWidgetHostView.
    virtual bool ShouldContinueHitTesting(
        RenderWidgetHostViewBase* target_view) const = 0;
  };

  // The delegate must outlive this targeter.
  explicit RenderWidgetTargeter(Delegate* delegate);
  ~RenderWidgetTargeter();

  // Finds the appropriate target inside |root_view| for |event|, and dispatches
  // it through the delegate. |event| is in the coord-space of |root_view|.
  void FindTargetAndDispatch(RenderWidgetHostViewBase* root_view,
                             const blink::WebInputEvent& event,
                             const ui::LatencyInfo& latency);

  // Finds the appropriate target inside |root_view| for |point|, and passes the
  // target along with the transformed coordinates of the point with respect to
  // the target's coordinates.
  void FindTargetAndCallback(RenderWidgetHostViewBase* root_view,
                             const gfx::PointF& point,
                             RenderWidgetHostAtPointCallback callback);

  void ViewWillBeDestroyed(RenderWidgetHostViewBase* view);

  bool HasEventsPendingDispatch() const;

  void set_async_hit_test_timeout_delay_for_testing(
      const base::TimeDelta& delay) {
    async_hit_test_timeout_delay_ = delay;
  }

  size_t num_requests_in_queue_for_testing() { return requests_.size(); }
  bool is_request_in_flight_for_testing() {
    return request_in_flight_.has_value();
  }

 private:
  class TargetingRequest {
   public:
    TargetingRequest(base::WeakPtr<RenderWidgetHostViewBase>,
                     const blink::WebInputEvent&,
                     const ui::LatencyInfo&);
    TargetingRequest(base::WeakPtr<RenderWidgetHostViewBase>,
                     const gfx::PointF&,
                     RenderWidgetHostAtPointCallback);
    TargetingRequest(TargetingRequest&& request);
    TargetingRequest& operator=(TargetingRequest&& other);
    ~TargetingRequest();

    void RunCallback(RenderWidgetHostViewBase* target,
                     base::Optional<gfx::PointF> point);

    bool MergeEventIfPossible(const blink::WebInputEvent& event);
    bool IsWebInputEventRequest() const;
    const blink::WebInputEvent& GetEvent() const;
    RenderWidgetHostViewBase* GetRootView() const;
    gfx::PointF GetLocation() const;
    const ui::LatencyInfo& GetLatency() const;

    // Queued TargetingRequest
    void StartQueueingTimeTracker();
    void StopQueueingTimeTracker();

    // Verification TargetingRequest
    viz::FrameSinkId GetExpectedFrameSinkId() const;
    void SetExpectedFrameSinkId(const viz::FrameSinkId& id);

   private:
    base::WeakPtr<RenderWidgetHostViewBase> root_view;

    RenderWidgetHostAtPointCallback callback;

    // |location| is in the coordinate space of |root_view| which is
    // either set directly when event is null or calculated from the event.
    gfx::PointF location;
    // |event| if set is in the coordinate space of |root_view|.
    ui::WebScopedInputEvent event;
    ui::LatencyInfo latency;

    // |expected_frame_sink_id| is only valid if the request is for
    // verification. It is temporarily added for v2 viz hit testing.
    // V2 uses cc generated hit test data and we need to verify its correctness.
    // The variable is the target frame sink id v2 finds in synchronous hit
    // testing. It should be the same as the async hit testing target if v2
    // works correctly.
    viz::FrameSinkId expected_frame_sink_id;

    // To track how long the request has been queued.
    std::unique_ptr<TracingUmaTracker> tracker;

    DISALLOW_COPY_AND_ASSIGN(TargetingRequest);
  };

  void ResolveTargetingRequest(TargetingRequest);

  // Attempts to target and dispatch all events in the queue. It stops if it has
  // to query a client, or if the queue becomes empty.
  void FlushEventQueue(bool is_verifying);

  // Queries |target| to find the correct target for |request|.
  // |target_location| is the location in |target|'s coordinate space.
  // |last_request_target| and |last_target_location| provide a fallback target
  // in the case that the query times out. These should be null values when
  // querying the root view, and the target's immediate parent view otherwise.
  void QueryClientInternal(RenderWidgetHostViewBase* target,
                           const gfx::PointF& target_location,
                           RenderWidgetHostViewBase* last_request_target,
                           const gfx::PointF& last_target_location,
                           TargetingRequest request);

  void QueryClient(TargetingRequest request);

  void QueryAndVerifyClient(TargetingRequest request);

  // |target_location|, if
  // set, is the location in |target|'s coordinate space.
  // |target| is the current target that will be queried using its
  // InputTargetClient interface.
  // |frame_sink_id| is returned from the InputTargetClient to indicate where
  // the event should be routed, and |transformed_location| is the point in
  // that new target's coordinate space.
  // |is_verification_request| is true if this targeting request was for
  // verification only.
  void FoundFrameSinkId(base::WeakPtr<RenderWidgetHostViewBase> target,
                        uint32_t request_id,
                        const gfx::PointF& target_location,
                        TracingUmaTracker tracker,
                        const bool is_verification_request,
                        const viz::FrameSinkId& frame_sink_id,
                        const gfx::PointF& transformed_location);

  // |target_location|, if
  // set, is the location in |target|'s coordinate space. If |latched_target| is
  // false, we explicitly did hit-testing for this event, instead of using a
  // known target.
  void FoundTarget(RenderWidgetHostViewBase* target,
                   const base::Optional<gfx::PointF>& target_location,
                   bool latched_target,
                   TargetingRequest* request);

  // Callback when the hit testing timer fires, to resume event processing
  // without further waiting for a response to the last targeting request.
  void AsyncHitTestTimedOut(
      base::WeakPtr<RenderWidgetHostViewBase> current_request_target,
      const gfx::PointF& current_target_location,
      base::WeakPtr<RenderWidgetHostViewBase> last_request_target,
      const gfx::PointF& last_target_location,
      const bool is_verification_request);

  base::TimeDelta async_hit_test_timeout_delay() {
    return async_hit_test_timeout_delay_;
  }

  base::Optional<TargetingRequest> request_in_flight_;
  uint32_t last_request_id_ = 0;
  std::queue<TargetingRequest> requests_;

  // With viz-hit-testing-surface-layer being enabled, we do async hit testing
  // for already dispatched events for verification. These verification requests
  // should not block normal hit testing requests.
  base::Optional<TargetingRequest> verify_request_in_flight_;
  uint32_t last_verify_request_id_ = 0;
  std::queue<TargetingRequest> verify_requests_;

  std::unordered_set<RenderWidgetHostViewBase*> unresponsive_views_;

  // This value keeps track of the number of clients we have asked in order to
  // do async hit-testing.
  uint32_t async_depth_ = 0;

  // This value limits how long to wait for a response from the renderer
  // process before giving up and resuming event processing.
  base::TimeDelta async_hit_test_timeout_delay_ =
      base::TimeDelta::FromMilliseconds(kAsyncHitTestTimeoutMs);

  std::unique_ptr<OneShotTimeoutMonitor> async_hit_test_timeout_;
  std::unique_ptr<OneShotTimeoutMonitor> async_verify_hit_test_timeout_;

  uint64_t trace_id_;

  const bool is_viz_hit_testing_debug_enabled_;
  // Stores SurfaceIds for regions that were async queried if hit-test debugging
  // is enabled. This allows us to send the queried regions in batches.
  std::vector<viz::FrameSinkId> hit_test_async_queried_debug_queue_;

  Delegate* const delegate_;
  base::WeakPtrFactory<RenderWidgetTargeter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetTargeter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_TARGETER_H_
