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
#include "platform/scheduler/test/fake_web_frame_scheduler.h"
#include "platform/scheduler/test/fake_web_task_runner.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderFactory.h"

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
    return security_origin_.get();
  }

  void SetSecurityOrigin(scoped_refptr<SecurityOrigin> security_origin) {
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
      FetchParameters::OriginRestriction,
      ResourceRequest::RedirectStatus redirect_status) const override {
    return ResourceRequestBlockedReason::kNone;
  }
  ResourceRequestBlockedReason CheckCSPForRequest(
      WebURLRequest::RequestContext,
      const KURL& url,
      const ResourceLoaderOptions& options,
      SecurityViolationReportingPolicy reporting_policy,
      ResourceRequest::RedirectStatus redirect_status) const override {
    return ResourceRequestBlockedReason::kNone;
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
      const ResourceRequest& request,
      scoped_refptr<WebTaskRunner> task_runner) override {
    if (!url_loader_factory_) {
      url_loader_factory_ =
          Platform::Current()->CreateDefaultURLLoaderFactory();
    }
    WrappedResourceRequest wrapped(request);
    return url_loader_factory_->CreateURLLoader(
        wrapped, task_runner->ToSingleThreadTaskRunner());
  }

  WebFrameScheduler* GetFrameScheduler() override {
    return frame_scheduler_.get();
  }

  scoped_refptr<WebTaskRunner> GetLoadingTaskRunner() override {
    return frame_scheduler_->GetTaskRunner(TaskType::kUnspecedLoading);
  }

 private:
  class MockFrameScheduler final : public scheduler::FakeWebFrameScheduler {
   public:
    MockFrameScheduler(scoped_refptr<WebTaskRunner> runner)
        : runner_(std::move(runner)) {}
    scoped_refptr<WebTaskRunner> GetTaskRunner(TaskType) override {
      return runner_;
    }

   private:
    scoped_refptr<WebTaskRunner> runner_;
  };

  MockFetchContext(LoadPolicy load_policy)
      : load_policy_(load_policy),
        runner_(base::MakeRefCounted<scheduler::FakeWebTaskRunner>()),
        security_origin_(SecurityOrigin::CreateUnique()),
        frame_scheduler_(new MockFrameScheduler(runner_)),
        complete_(false),
        transfer_size_(-1) {}

  enum LoadPolicy load_policy_;
  scoped_refptr<WebTaskRunner> runner_;
  scoped_refptr<SecurityOrigin> security_origin_;
  std::unique_ptr<WebFrameScheduler> frame_scheduler_;
  std::unique_ptr<WebURLLoaderFactory> url_loader_factory_;
  bool complete_;
  long long transfer_size_;
};

}  // namespace blink

#endif
