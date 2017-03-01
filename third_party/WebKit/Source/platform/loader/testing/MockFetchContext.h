// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockFetchContext_h
#define MockFetchContext_h

#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/scheduler/test/fake_web_task_runner.h"
#include "wtf/PtrUtil.h"

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
  // TODO(toyoshim): Disallow to pass nullptr for |taskRunner|, and force to use
  // FetchTestingPlatformSupport's WebTaskRunner. Probably, MockFetchContext
  // would be available only through the FetchTestingPlatformSupport in the
  // future.
  static MockFetchContext* create(LoadPolicy loadPolicy,
                                  RefPtr<WebTaskRunner> taskRunner = nullptr) {
    return new MockFetchContext(loadPolicy, std::move(taskRunner));
  }

  ~MockFetchContext() override {}

  void setLoadComplete(bool complete) { m_complete = complete; }
  long long getTransferSize() const { return m_transferSize; }

  // FetchContext:
  bool allowImage(bool imagesEnabled, const KURL&) const override {
    return true;
  }
  ResourceRequestBlockedReason canRequest(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      FetchRequest::OriginRestriction) const override {
    return ResourceRequestBlockedReason::None;
  }
  bool shouldLoadNewResource(Resource::Type) const override {
    return m_loadPolicy == kShouldLoadNewResource;
  }
  RefPtr<WebTaskRunner> loadingTaskRunner() const override { return m_runner; }
  bool isLoadComplete() const override { return m_complete; }
  void addResourceTiming(
      const ResourceTimingInfo& resourceTimingInfo) override {
    m_transferSize = resourceTimingInfo.transferSize();
  }

 private:
  MockFetchContext(LoadPolicy loadPolicy, RefPtr<WebTaskRunner> taskRunner)
      : m_loadPolicy(loadPolicy),
        m_runner(taskRunner ? std::move(taskRunner)
                            : adoptRef(new scheduler::FakeWebTaskRunner)),
        m_complete(false),
        m_transferSize(-1) {}

  enum LoadPolicy m_loadPolicy;
  RefPtr<WebTaskRunner> m_runner;
  bool m_complete;
  long long m_transferSize;
};

}  // namespace blink

#endif
