// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_FRAME_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_FRAME_SCHEDULER_H_

#include <deque>

#include "platform/WebFrameScheduler.h"
#include "platform/WebTaskRunner.h"
#include "platform/scheduler/renderer/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

class MainThreadTaskQueueForTest : public MainThreadTaskQueue {
 public:
  MainThreadTaskQueueForTest(QueueType queue_type)
      : MainThreadTaskQueue(nullptr,
                            Spec(MainThreadTaskQueue::NameForQueueType(
                                MainThreadTaskQueue::QueueType::TEST)),
                            QueueCreationParams(queue_type),
                            nullptr) {}
  ~MainThreadTaskQueueForTest() {}
};

// A dummy WebFrameScheduler for tests.
class FakeWebFrameScheduler : public WebFrameScheduler {
 public:
  FakeWebFrameScheduler()
      : web_view_scheduler_(nullptr),
        is_page_visible_(false),
        is_frame_visible_(false),
        frame_type_(WebFrameScheduler::FrameType::kSubframe),
        is_cross_origin_(false),
        is_exempt_from_throttling_(false) {}
  FakeWebFrameScheduler(WebViewScheduler* web_view_scheduler,
                        bool is_page_visible,
                        bool is_frame_visible,
                        WebFrameScheduler::FrameType frame_type,
                        bool is_cross_origin,
                        bool is_exempt_from_throttling)
      : web_view_scheduler_(web_view_scheduler),
        is_page_visible_(is_page_visible),
        is_frame_visible_(is_frame_visible),
        frame_type_(frame_type),
        is_cross_origin_(is_cross_origin),
        is_exempt_from_throttling_(is_exempt_from_throttling) {
    DCHECK(frame_type_ != FrameType::kMainFrame || !is_cross_origin);
  }
  ~FakeWebFrameScheduler() override {}

  class Builder {
   public:
    Builder() {}

    std::unique_ptr<FakeWebFrameScheduler> Build() {
      return std::make_unique<FakeWebFrameScheduler>(
          web_view_scheduler_, is_page_visible_, is_frame_visible_, frame_type_,
          is_cross_origin_, is_exempt_from_throttling_);
    }

    Builder& SetWebViewScheduler(WebViewScheduler* web_view_scheduler) {
      web_view_scheduler_ = web_view_scheduler;
      return *this;
    }

    Builder& SetIsPageVisible(bool is_page_visible) {
      is_page_visible_ = is_page_visible;
      return *this;
    }

    Builder& SetIsFrameVisible(bool is_frame_visible) {
      is_frame_visible_ = is_frame_visible;
      return *this;
    }

    Builder& SetFrameType(WebFrameScheduler::FrameType frame_type) {
      frame_type_ = frame_type;
      return *this;
    }

    Builder& SetIsCrossOrigin(bool is_cross_origin) {
      is_cross_origin_ = is_cross_origin;
      return *this;
    }

    Builder& SetIsExemptFromThrottling(bool is_exempt_from_throttling) {
      is_exempt_from_throttling_ = is_exempt_from_throttling;
      return *this;
    }

   private:
    WebViewScheduler* web_view_scheduler_ = nullptr;
    bool is_page_visible_ = false;
    bool is_frame_visible_ = false;
    WebFrameScheduler::FrameType frame_type_ =
        WebFrameScheduler::FrameType::kMainFrame;
    bool is_cross_origin_ = false;
    bool is_exempt_from_throttling_ = false;
  };

  // WebFrameScheduler implementation:
  void AddThrottlingObserver(ObserverType, Observer*) override {}
  void RemoveThrottlingObserver(ObserverType, Observer*) override {}
  void SetFrameVisible(bool) override {}
  bool IsFrameVisible() const override { return is_frame_visible_; }
  void SetPageVisible(bool) override {}
  bool IsPageVisible() const override { return is_page_visible_; }
  void SetPaused(bool) override {}
  void SetCrossOrigin(bool) override {}
  bool IsCrossOrigin() const override { return is_cross_origin_; }
  WebFrameScheduler::FrameType GetFrameType() const override {
    return frame_type_;
  }
  scoped_refptr<WebTaskRunner> GetTaskRunner(TaskType) override {
    return nullptr;
  }
  WebViewScheduler* GetWebViewScheduler() const override {
    return web_view_scheduler_;
  }
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
  bool IsExemptFromBudgetBasedThrottling() const override {
    return is_exempt_from_throttling_;
  }

 private:
  WebViewScheduler* web_view_scheduler_;  // NOT OWNED

  bool is_page_visible_;
  bool is_frame_visible_;
  WebFrameScheduler::FrameType frame_type_;
  bool is_cross_origin_;
  bool is_exempt_from_throttling_;
  DISALLOW_COPY_AND_ASSIGN(FakeWebFrameScheduler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_FRAME_SCHEDULER_H_
