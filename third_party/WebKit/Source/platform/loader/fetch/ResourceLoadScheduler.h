// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResourceLoadScheduler_h
#define ResourceLoadScheduler_h

#include <set>
#include "platform/WebFrameScheduler.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/wtf/HashSet.h"

namespace blink {

// Client interface to use the throttling/scheduling functionality that
// ResourceLoadScheduler provides.
class PLATFORM_EXPORT ResourceLoadSchedulerClient
    : public GarbageCollectedMixin {
 public:
  // Called when the request is granted to run.
  virtual void Run() = 0;

  void Trace(blink::Visitor* visitor) override {}
};

// The ResourceLoadScheduler provides a unified per-frame infrastructure to
// schedule loading requests. It receives resource requests as
// ResourceLoadSchedulerClient instances and calls Run() on them to dispatch
// them possibly with additional throttling/scheduling. This also keeps track of
// in-flight requests that are granted but are not released (by Release()) yet.
// Note: If FetchContext can not provide a WebFrameScheduler, throttling and
// scheduling functionalities will be completely disabled.
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

    // Takes a shared instance to represent an invalid instance that will be
    // used when a caller don't want to report traffic, i.e. on a failure.
    static PLATFORM_EXPORT TrafficReportHints& InvalidInstance();

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

  // Returned on Request(). Caller should need to return it via Release().
  using ClientId = uint64_t;

  static constexpr ClientId kInvalidClientId = 0u;

  static constexpr size_t kOutstandingUnlimited =
      std::numeric_limits<size_t>::max();

  static ResourceLoadScheduler* Create(FetchContext* context = nullptr) {
    return new ResourceLoadScheduler(context ? context
                                             : &FetchContext::NullInstance());
  }
  ~ResourceLoadScheduler();
  void Trace(blink::Visitor*);

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

  // Sets outstanding limit for testing.
  void SetOutstandingLimitForTesting(size_t limit);

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
  void Run(ClientId, ResourceLoadSchedulerClient*);

  void SetOutstandingLimitAndMaybeRun(size_t limit);

  // A flag to indicate an internal running state.
  // TODO(toyoshim): We may want to use enum once we start to have more states.
  bool is_shutdown_ = false;

  // A mutable flag to indicate if the throttling and scheduling are enabled.
  // Can be modified by field trial flags or for testing.
  bool is_enabled_ = false;

  // Outstanding limit. 0u means unlimited.
  // TODO(crbug.com/735410): If this throttling is enabled always, it makes some
  // tests fail.
  size_t outstanding_limit_ = kOutstandingUnlimited;

  // Outstanding limit for throttled frames. Managed via the field trial.
  const size_t outstanding_throttled_limit_;

  // The last used ClientId to calculate the next.
  ClientId current_id_ = kInvalidClientId;

  // Holds clients that were granted and are running.
  HashSet<ClientId> running_requests_;

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
};

}  // namespace blink

#endif
