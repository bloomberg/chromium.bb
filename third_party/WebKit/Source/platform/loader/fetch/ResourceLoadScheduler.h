// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResourceLoadScheduler_h
#define ResourceLoadScheduler_h

#include "platform/heap/GarbageCollected.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/HashSet.h"

namespace blink {

// Client interface to use the throttling/scheduling functionality that
// ResourceLoadScheduler provides.
class PLATFORM_EXPORT ResourceLoadSchedulerClient
    : public GarbageCollectedMixin {
 public:
  // Called when the request is granted to run.
  virtual void Run() = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

// The ResourceLoadScheduler provides a unified per-frame infrastructure to
// schedule loading requests. It receives resource requests as
// ResourceLoadSchedulerClient instances and calls Run() on them to dispatch
// them possibly with additional throttling/scheduling. This also keeps track of
// in-flight requests that are granted but are not released (by Release()) yet.
class PLATFORM_EXPORT ResourceLoadScheduler final
    : public GarbageCollectedFinalized<ResourceLoadScheduler> {
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

  // Returned on Request(). Caller should need to return it via Release().
  using ClientId = uint64_t;

  static constexpr ClientId kInvalidClientId = 0u;

  static ResourceLoadScheduler* Create() { return new ResourceLoadScheduler(); }
  ~ResourceLoadScheduler() {}
  DECLARE_TRACE();

  // Makes a request. ClientId should be set before Run() is invoked so that
  // caller can call Release() with the assigned ClientId correctly even if
  // the invocation happens synchronously.
  void Request(ResourceLoadSchedulerClient*, ThrottleOption, ClientId*);

  // ResourceLoadSchedulerClient should call this method when the loading is
  // finished, or canceled. This method can be called in a pre-finalization
  // step, bug the ReleaseOption must be kReleaseOnly in such a case.
  bool Release(ClientId, ReleaseOption);

  // Sets outstanding limit for testing.
  void SetOutstandingLimitForTesting(size_t limit);

 private:
  ResourceLoadScheduler();

  // Generates the next ClientId.
  ClientId GenerateClientId();

  // Picks up one client if there is a budget and route it to run.
  void MaybeRun();

  // Grants a client to run,
  void Run(ClientId, ResourceLoadSchedulerClient*);

  // Outstanding limit. 0u means unlimited.
  // TODO(crbug.com/735410): If this throttling is enabled always, it makes some
  // tests fail.
  size_t outstanding_limit_ = 0;

  // The last used ClientId to calculate the next.
  ClientId current_id_ = kInvalidClientId;

  // Holds clients that were granted and are running.
  HashSet<ClientId> running_requests_;

  // Holds clients that haven't been granted, and are waiting for a grant.
  HeapHashMap<ClientId, Member<ResourceLoadSchedulerClient>>
      pending_request_map_;
  Deque<ClientId> pending_request_queue_;
};

}  // namespace blink

#endif
