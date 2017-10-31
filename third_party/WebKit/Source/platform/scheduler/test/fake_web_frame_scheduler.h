// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_FRAME_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_FRAME_SCHEDULER_H_

#include <deque>

#include "platform/WebFrameScheduler.h"

namespace blink {
namespace scheduler {

// A dummy WebFrameScheduler for tests.
class FakeWebFrameScheduler : public WebFrameScheduler {
 public:
  FakeWebFrameScheduler() {}
  ~FakeWebFrameScheduler() override {}

  // WebFrameScheduler implementation:
  void AddThrottlingObserver(ObserverType, Observer*) override {}
  void RemoveThrottlingObserver(ObserverType, Observer*) override {}
  void SetFrameVisible(bool) override {}
  bool IsFrameVisible() const override { return false; }
  void SetPageVisible(bool) override {}
  bool IsPageVisible() const override { return false; }
  void SetPaused(bool) override {}
  void SetCrossOrigin(bool) override {}
  bool IsCrossOrigin() const override { return false; }
  WebFrameScheduler::FrameType GetFrameType() const override {
    return WebFrameScheduler::FrameType::kSubframe;
  }
  scoped_refptr<WebTaskRunner> GetTaskRunner(TaskType) override {
    return nullptr;
  }
  WebViewScheduler* GetWebViewScheduler() override { return nullptr; }
  ScopedVirtualTimePauser CreateScopedVirtualTimePauser() {
    return ScopedVirtualTimePauser();
  }
  void DidStartProvisionalLoad(bool is_main_frame) override {}
  void DidCommitProvisionalLoad(bool is_web_history_inert_commit,
                                bool is_reload,
                                bool is_main_frame) override {}
  void OnFirstMeaningfulPaint() override {}
  std::unique_ptr<ActiveConnectionHandle> OnActiveConnectionCreated() override {
    return nullptr;
  }
  bool IsExemptFromThrottling() const override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeWebFrameScheduler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_FRAME_SCHEDULER_H_
