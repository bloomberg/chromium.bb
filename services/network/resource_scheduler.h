// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_RESOURCE_SCHEDULER_H_
#define SERVICES_NETWORK_RESOURCE_SCHEDULER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "net/base/priority_queue.h"
#include "net/base/request_priority.h"
#include "net/nqe/effective_connection_type.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {
class URLRequest;
class NetworkQualityEstimator;
}  // namespace net

namespace network {

class ResourceSchedulerParamsManager;

// There is one ResourceScheduler. All renderer-initiated HTTP requests are
// expected to pass through it.
//
// There are two types of input to the scheduler:
// 1. Requests to start, cancel, or finish fetching a resource.
// 2. Notifications for renderer events, such as new tabs, navigation and
//    painting.
//
// These input come from different threads, so they may not be in sync. The UI
// thread is considered the authority on renderer lifetime, which means some
// IPCs may be meaningless if they arrive after the UI thread signals a renderer
// has been deleted.
//
// The ResourceScheduler tracks many Clients, which should correlate with tabs.
// A client is uniquely identified by its child_id and route_id.
//
// Each Client may have many Requests in flight. Requests are uniquely
// identified within a Client by its ScheduledResourceRequest.
//
// Users should call ScheduleRequest() to notify this ResourceScheduler of a new
// request. The returned ResourceThrottle should be destroyed when the load
// finishes or is canceled, before the net::URLRequest.
//
// The scheduler may defer issuing the request via the ResourceThrottle
// interface or it may alter the request's priority by calling set_priority() on
// the URLRequest.
class COMPONENT_EXPORT(NETWORK_SERVICE) ResourceScheduler {
 public:
  class ScheduledResourceRequest {
   public:
    ScheduledResourceRequest();
    virtual ~ScheduledResourceRequest();
    virtual void WillStartRequest(bool* defer) = 0;

    void set_resume_callback(base::OnceClosure callback) {
      resume_callback_ = std::move(callback);
    }
    void RunResumeCallback();

   private:
    base::OnceClosure resume_callback_;
  };

  explicit ResourceScheduler(bool enabled);
  ~ResourceScheduler();

  // Requests that this ResourceScheduler schedule, and eventually loads, the
  // specified |url_request|. Caller should delete the returned ResourceThrottle
  // when the load completes or is canceled, before |url_request| is deleted.
  std::unique_ptr<ScheduledResourceRequest> ScheduleRequest(
      int child_id,
      int route_id,
      bool is_async,
      net::URLRequest* url_request);

  // Signals from the UI thread, posted as tasks on the IO thread:

  // Called when a renderer is created. |network_quality_estimator| is allowed
  // to be null.
  void OnClientCreated(
      int child_id,
      int route_id,
      const net::NetworkQualityEstimator* const network_quality_estimator);

  // Called when a renderer is destroyed.
  void OnClientDeleted(int child_id, int route_id);

  // Called when a renderer stops or restarts loading.
  // Do not call this function when the network service is enabled.
  void DeprecatedOnLoadingStateChanged(int child_id,
                                       int route_id,
                                       bool is_loaded);

  // Signals from IPC messages directly from the renderers:

  // Called when a client navigates to a new main document.
  // Do not call this function when the network service is enabled.
  void DeprecatedOnNavigate(int child_id, int route_id);

  // Called when the client has parsed the <body> element. This is a signal that
  // resource loads won't interfere with first paint.
  // Do not call this function when the network service is enabled.
  void DeprecatedOnWillInsertBody(int child_id, int route_id);

  // Signals from the IO thread:

  // Called when we received a response to a http request that was served
  // from a proxy using SPDY.
  void OnReceivedSpdyProxiedHttpResponse(int child_id, int route_id);

  // Client functions:

  // Returns true if at least one client is currently loading.
  // Do not call this function when the network service is enabled.
  bool DeprecatedHasLoadingClients() const;

  // Updates the priority for |request|. Modifies request->priority(), and may
  // start the request loading if it wasn't already started.
  // If the scheduler does not know about the request, |new_priority| is set but
  // |intra_priority_value| is ignored.
  void ReprioritizeRequest(net::URLRequest* request,
                           net::RequestPriority new_priority,
                           int intra_priority_value);
  // Same as above, but keeps the existing intra priority value.
  void ReprioritizeRequest(net::URLRequest* request,
                           net::RequestPriority new_priority);

  bool priority_requests_delayable() const {
    return priority_requests_delayable_;
  }
  bool head_priority_requests_delayable() const {
    return head_priority_requests_delayable_;
  }
  bool yielding_scheduler_enabled() const {
    return yielding_scheduler_enabled_;
  }
  int max_requests_before_yielding() const {
    return max_requests_before_yielding_;
  }
  base::TimeDelta yield_time() const { return yield_time_; }
  base::SequencedTaskRunner* task_runner() { return task_runner_.get(); }

  // Testing setters
  void SetMaxRequestsBeforeYieldingForTesting(
      int max_requests_before_yielding) {
    max_requests_before_yielding_ = max_requests_before_yielding;
  }
  void SetYieldTimeForTesting(base::TimeDelta yield_time) {
    yield_time_ = yield_time;
  }
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) {
    task_runner_ = std::move(sequenced_task_runner);
  }

  bool enabled() const { return enabled_; }

  static bool IsRendererSideResourceSchedulerEnabled();

  void SetResourceSchedulerParamsManagerForTests(
      std::unique_ptr<ResourceSchedulerParamsManager>
          resource_scheduler_params_manager);

 private:
  class Client;
  class RequestQueue;
  class ScheduledResourceRequestImpl;
  struct RequestPriorityParams;
  struct ScheduledResourceSorter {
    bool operator()(const ScheduledResourceRequestImpl* a,
                    const ScheduledResourceRequestImpl* b) const;
  };

  using ClientId = int64_t;
  using ClientMap = std::map<ClientId, std::unique_ptr<Client>>;
  using RequestSet = std::set<ScheduledResourceRequestImpl*>;

  // Called when a ScheduledResourceRequest is destroyed.
  void RemoveRequest(ScheduledResourceRequestImpl* request);

  // Returns the client ID for the given |child_id| and |route_id| combo.
  ClientId MakeClientId(int child_id, int route_id);

  // Returns the client for the given |child_id| and |route_id| combo.
  Client* GetClient(int child_id, int route_id);

  ClientMap client_map_;
  RequestSet unowned_requests_;

  // Whether or not to enable ResourceScheduling. This will almost always be
  // enabled, except for some C++ headless embedders who may implement their own
  // resource scheduling via protocol handlers.
  const bool enabled_;

  // True if requests to servers that support priorities (e.g., H2/QUIC) can
  // be delayed.
  bool priority_requests_delayable_;

  // True if requests to servers that support priorities (e.g., H2/QUIC) can
  // be delayed while the parser is in head.
  bool head_priority_requests_delayable_;

  // True if the scheduler should yield between several successive calls to
  // start resource requests.
  bool yielding_scheduler_enabled_;
  int max_requests_before_yielding_;
  base::TimeDelta yield_time_;

  std::unique_ptr<ResourceSchedulerParamsManager>
      resource_scheduler_params_manager_;

  // The TaskRunner to post tasks on. Can be overridden for tests.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ResourceScheduler);
};

}  // namespace network

#endif  // SERVICES_NETWORK_RESOURCE_SCHEDULER_H_
