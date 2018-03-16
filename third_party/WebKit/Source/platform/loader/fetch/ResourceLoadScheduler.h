// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResourceLoadScheduler_h
#define ResourceLoadScheduler_h

#include <set>
#include "platform/WebFrameScheduler.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/wtf/HashSet.h"

namespace blink {

class FetchContext;

// Client interface to use the throttling/scheduling functionality that
// ResourceLoadScheduler provides.
class PLATFORM_EXPORT ResourceLoadSchedulerClient
    : public GarbageCollectedMixin {
 public:
  // Called when the request is granted to run.
  virtual void Run() = 0;

  void Trace(blink::Visitor* visitor) override {}
};

// ResourceLoadScheduler provides a unified per-frame infrastructure to schedule
// loading requests. When Request() is called with a
// ResourceLoadSchedulerClient |client|, it calls |client|'s Run() method
// synchronously or asynchronously to notify that |client| can start loading.
//
// A ResourceLoadScheduler may initiate a new resource loading in the following
// cases:
// - When Request() is called
// - When LoosenThrottlingPolicy() is called
// - When SetPriority() is called
// - When Release() is called with kReleaseAndSchedule
// - When OnThrottlingStateChanged() is called
//
// A ResourceLoadScheduler determines if a request can be throttable or not, and
// keeps track of pending throttable requests with priority information (i.e.,
// ResourceLoadPriority accompanied with an integer called "intra-priority").
// Here are the general principles:
//  - A ResourceLoadScheduler does not throttle requests that cannot be
//    throttable. It will call client's Run() method as soon as possible.
//  - A ResourceLoadScheduler determines whether a request can be throttable by
//    seeing Request()'s ThrottleOption argument and requests' priority
//    information. Requests' priority information can be modified via
//    SetPriority().
//  - A ResourceLoadScheulder won't initiate a new resource loading which can
//    be throttable when there are active resource loading activities more than
//    its internal threshold (i.e., what GetOutstandingLimit() returns)".
//
// By default, ResourceLoadScheduler is disabled, which means it doesn't
// throttle any resource loading requests.
//
// Here are running experiments (as of M65):
//  - "ResourceLoadScheduler"
//   - Resource loading requests are not at throttled when the frame is in
//     the foreground tab.
//   - Resource loading requests are throttled when the frame is in a
//     background tab. It has different thresholds for the main frame
//     and sub frames. When the frame has been background for more than five
//     minutes, all throttable resource loading requests are throttled
//     indefinitely (i.e., threshold is zero in such a circumstance).
//  - RendererSideResourceScheduler
//    ResourceLoadScheduler has two modes each of which has its own threshold.
//    - Tight mode (used until the frame sees a <body> element):
//      ResourceLoadScheduler considers a request throttable if its priority
//      is less than |kHigh|.
//    - Normal mode:
//      ResourceLoadScheduler considers a request throttable if its priority
//      is less than |kMedium|.
class PLATFORM_EXPORT ResourceLoadScheduler final
    : public GarbageCollectedFinalized<ResourceLoadScheduler>,
      public WebFrameScheduler::Observer {
  WTF_MAKE_NONCOPYABLE(ResourceLoadScheduler);

 public:
  // An option to use in calling Request(). If kCanNotBeThrottled is specified,
  // the request should be granted and Run() should be called synchronously.
  // Otherwise, OnRequestGranted() could be called later when other outstanding
  // requests are finished.
  enum class ThrottleOption { kCanBeThrottled, kCanNotBeThrottled };

  // An option to use in calling Release(). If kReleaseOnly is specified,
  // the specified request should be released, but no other requests should
  // be scheduled within the call.
  enum class ReleaseOption { kReleaseOnly, kReleaseAndSchedule };

  // A class to pass traffic report hints on calling Release().
  class TrafficReportHints {
   public:
    // |encoded_data_length| is payload size in bytes sent over the network.
    // |decoded_body_length| is received resource data size in bytes.
    TrafficReportHints(int64_t encoded_data_length, int64_t decoded_body_length)
        : valid_(true),
          encoded_data_length_(encoded_data_length),
          decoded_body_length_(decoded_body_length) {}

    // Returns the instance that represents an invalid report, which can be
    // used when a caller don't want to report traffic, i.e. on a failure.
    static PLATFORM_EXPORT TrafficReportHints InvalidInstance() {
      return TrafficReportHints();
    }

    bool IsValid() const { return valid_; }

    int64_t encoded_data_length() const {
      DCHECK(valid_);
      return encoded_data_length_;
    }
    int64_t decoded_body_length() const {
      DCHECK(valid_);
      return decoded_body_length_;
    }

   private:
    // Default constructor makes an invalid instance that won't be recorded.
    TrafficReportHints() = default;

    bool valid_ = false;
    int64_t encoded_data_length_ = 0;
    int64_t decoded_body_length_ = 0;
  };

  // ResourceLoadScheduler has two policies: |kTight| and |kNormal|. Currently
  // this is used to support aggressive throttling while the corresponding frame
  // is in layout-blocking phase. There is only one state transition,
  // |kTight| => |kNormal|, which is done by |LoosenThrottlingPolicy|.
  enum class ThrottlingPolicy { kTight, kNormal };

  // Returned on Request(). Caller should need to return it via Release().
  using ClientId = uint64_t;

  static constexpr ClientId kInvalidClientId = 0u;

  static constexpr size_t kOutstandingUnlimited =
      std::numeric_limits<size_t>::max();

  static ResourceLoadScheduler* Create(FetchContext* = nullptr);
  ~ResourceLoadScheduler();

  void Trace(blink::Visitor*);

  // Changes the policy from |kTight| to |kNormal|. This function can be called
  // multiple times, and does nothing when the scheduler is already working with
  // the normal policy. This function may initiate a new resource loading.
  void LoosenThrottlingPolicy();

  // Stops all operations including observing throttling signals.
  // ResourceLoadSchedulerClient::Run() will not be called once this method is
  // called. This method can be called multiple times safely.
  void Shutdown();

  // Makes a request. This may synchronously call
  // ResourceLoadSchedulerClient::Run(), but it is guaranteed that ClientId is
  // populated before ResourceLoadSchedulerClient::Run() is called, so that the
  // caller can call Release() with the assigned ClientId correctly.
  void Request(ResourceLoadSchedulerClient*,
               ThrottleOption,
               ResourceLoadPriority,
               int intra_priority,
               ClientId*);

  // Updates the priority information of the given client. This function may
  // initiate a new resource loading.
  void SetPriority(ClientId, ResourceLoadPriority, int intra_priority);

  // ResourceLoadSchedulerClient should call this method when the loading is
  // finished, or canceled. This method can be called in a pre-finalization
  // step, bug the ReleaseOption must be kReleaseOnly in such a case.
  // TrafficReportHints is for reporting histograms.
  // TrafficReportHints::InvalidInstance() can be used to omit reporting.
  bool Release(ClientId, ReleaseOption, const TrafficReportHints&);

  // Checks if the specified client was already scheduled to call Run(), but
  // haven't call Release() yet.
  bool IsRunning(ClientId id) { return running_requests_.Contains(id); }

  // Sets outstanding limit for testing.
  void SetOutstandingLimitForTesting(size_t limit) {
    SetOutstandingLimitForTesting(limit, limit);
  }
  void SetOutstandingLimitForTesting(size_t tight_limit, size_t normal_limit);

  void OnNetworkQuiet();

  // Returns whether we can throttle a request with the given priority.
  // This function returns false when RendererSideResourceScheduler is disabled.
  bool IsThrottablePriority(ResourceLoadPriority) const;

  // WebFrameScheduler::Observer overrides:
  void OnThrottlingStateChanged(WebFrameScheduler::ThrottlingState) override;

 private:
  class TrafficMonitor;

  class ClientIdWithPriority {
   public:
    struct Compare {
      bool operator()(const ClientIdWithPriority& x,
                      const ClientIdWithPriority& y) const {
        if (x.priority != y.priority)
          return x.priority > y.priority;
        if (x.intra_priority != y.intra_priority)
          return x.intra_priority > y.intra_priority;
        return x.client_id < y.client_id;
      }
    };

    ClientIdWithPriority(ClientId client_id,
                         WebURLRequest::Priority priority,
                         int intra_priority)
        : client_id(client_id),
          priority(priority),
          intra_priority(intra_priority) {}

    const ClientId client_id;
    const WebURLRequest::Priority priority;
    const int intra_priority;
  };

  struct ClientWithPriority : public GarbageCollected<ClientWithPriority> {
    ClientWithPriority(ResourceLoadSchedulerClient* client,
                       ResourceLoadPriority priority,
                       int intra_priority)
        : client(client), priority(priority), intra_priority(intra_priority) {}

    void Trace(blink::Visitor* visitor) { visitor->Trace(client); }

    Member<ResourceLoadSchedulerClient> client;
    ResourceLoadPriority priority;
    int intra_priority;
  };

  ResourceLoadScheduler(FetchContext*);

  // Generates the next ClientId.
  ClientId GenerateClientId();

  // Picks up one client if there is a budget and route it to run.
  void MaybeRun();

  // Grants a client to run,
  void Run(ClientId, ResourceLoadSchedulerClient*, bool throttlable);

  size_t GetOutstandingLimit() const;

  // A flag to indicate an internal running state.
  // TODO(toyoshim): We may want to use enum once we start to have more states.
  bool is_shutdown_ = false;

  // A mutable flag to indicate if the throttling and scheduling are enabled.
  // Can be modified by field trial flags or for testing.
  bool is_enabled_ = false;

  ThrottlingPolicy policy_ = ThrottlingPolicy::kNormal;

  // ResourceLoadScheduler threshold values for various circumstances. Some
  // conditions can overlap, and ResourceLoadScheduler chooses the smallest
  // value in such cases.

  // Used when |policy_| is |kTight|.
  size_t tight_outstanding_limit_ = kOutstandingUnlimited;

  // Used when |policy_| is |kNormal|.
  size_t normal_outstanding_limit_ = kOutstandingUnlimited;

  // Used when |frame_scheduler_throttling_state_| is |kThrottled|.
  const size_t outstanding_limit_for_throttled_frame_scheduler_;

  // The last used ClientId to calculate the next.
  ClientId current_id_ = kInvalidClientId;

  // Holds clients that were granted and are running.
  HashSet<ClientId> running_requests_;

  HashSet<ClientId> running_throttlable_requests_;

  // Largest number of running requests seen so far.
  unsigned maximum_running_requests_seen_ = 0;

  enum class ThrottlingHistory {
    kInitial,
    kThrottled,
    kNotThrottled,
    kPartiallyThrottled,
    kStopped,
  };
  ThrottlingHistory throttling_history_ = ThrottlingHistory::kInitial;
  WebFrameScheduler::ThrottlingState frame_scheduler_throttling_state_ =
      WebFrameScheduler::ThrottlingState::kNotThrottled;

  // Holds clients that haven't been granted, and are waiting for a grant.
  HeapHashMap<ClientId, Member<ClientWithPriority>> pending_request_map_;
  // We use std::set here because WTF doesn't have its counterpart.
  std::set<ClientIdWithPriority, ClientIdWithPriority::Compare>
      pending_requests_;

  // Holds an internal class instance to monitor and report traffic.
  std::unique_ptr<TrafficMonitor> traffic_monitor_;

  // Holds FetchContext reference to contact WebFrameScheduler.
  Member<FetchContext> context_;

  // Handle to throttling observer.
  std::unique_ptr<WebFrameScheduler::ThrottlingObserverHandle>
      scheduler_observer_handle_;
};

}  // namespace blink

#endif
