// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockFetchContext_h
#define MockFetchContext_h

#include "platform/WebFrameScheduler.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/scheduler/test/fake_web_task_runner.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"

#include <memory>

namespace blink {

class KURL;
class ResourceRequest;
class WebTaskRunner;
struct ResourceLoaderOptions;

// Mocked FetchContext for testing.
class MockFetchContext : public FetchContext {
 public:
  enum LoadPolicy {
    kShouldLoadNewResource,
    kShouldNotLoadNewResource,
  };
  static MockFetchContext* Create(LoadPolicy load_policy) {
    return new MockFetchContext(load_policy);
  }

  ~MockFetchContext() override {}

  void SetLoadComplete(bool complete) { complete_ = complete; }
  long long GetTransferSize() const { return transfer_size_; }

  SecurityOrigin* GetSecurityOrigin() const override {
    return security_origin_.Get();
  }

  void SetSecurityOrigin(RefPtr<SecurityOrigin> security_origin) {
    security_origin_ = security_origin;
  }

  // FetchContext:
  bool AllowImage(bool images_enabled, const KURL&) const override {
    return true;
  }
  ResourceRequestBlockedReason CanRequest(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      FetchParameters::OriginRestriction) const override {
    return ResourceRequestBlockedReason::kNone;
  }
  ResourceRequestBlockedReason CanFollowRedirect(
      Resource::Type type,
      const ResourceRequest& request,
      const KURL& url,
      const ResourceLoaderOptions& options,
      SecurityViolationReportingPolicy reporting_policy,
      FetchParameters::OriginRestriction origin_restriction) const override {
    return CanRequest(type, request, url, options, reporting_policy,
                      origin_restriction);
  }
  bool ShouldLoadNewResource(Resource::Type) const override {
    return load_policy_ == kShouldLoadNewResource;
  }
  bool IsLoadComplete() const override { return complete_; }
  void AddResourceTiming(
      const ResourceTimingInfo& resource_timing_info) override {
    transfer_size_ = resource_timing_info.TransferSize();
  }

  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest& request) override {
    WrappedResourceRequest wrapped(request);
    return Platform::Current()->CreateURLLoader(
        wrapped, runner_->ToSingleThreadTaskRunner());
  }

  WebFrameScheduler* GetFrameScheduler() override {
    return frame_scheduler_.get();
  }

 private:
  class MockFrameScheduler final : public WebFrameScheduler {
   public:
    MockFrameScheduler(RefPtr<WebTaskRunner> runner)
        : runner_(std::move(runner)) {}
    void AddThrottlingObserver(ObserverType, Observer*) override {}
    void RemoveThrottlingObserver(ObserverType, Observer*) override {}
    RefPtr<WebTaskRunner> TimerTaskRunner() override { return runner_; }
    RefPtr<WebTaskRunner> LoadingTaskRunner() override { return runner_; }
    RefPtr<WebTaskRunner> LoadingControlTaskRunner() override {
      return runner_;
    }
    RefPtr<WebTaskRunner> SuspendableTaskRunner() override { return runner_; }
    RefPtr<WebTaskRunner> UnthrottledTaskRunner() override { return runner_; }
    RefPtr<WebTaskRunner> UnthrottledButBlockableTaskRunner() override {
      return runner_;
    }

   private:
    RefPtr<WebTaskRunner> runner_;
  };

  MockFetchContext(LoadPolicy load_policy)
      : load_policy_(load_policy),
        runner_(AdoptRef(new scheduler::FakeWebTaskRunner)),
        security_origin_(SecurityOrigin::CreateUnique()),
        frame_scheduler_(new MockFrameScheduler(runner_)),
        complete_(false),
        transfer_size_(-1) {}

  enum LoadPolicy load_policy_;
  RefPtr<WebTaskRunner> runner_;
  RefPtr<SecurityOrigin> security_origin_;
  std::unique_ptr<WebFrameScheduler> frame_scheduler_;
  bool complete_;
  long long transfer_size_;
};

}  // namespace blink

#endif
