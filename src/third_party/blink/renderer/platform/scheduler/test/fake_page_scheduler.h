// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_FAKE_PAGE_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_FAKE_PAGE_SCHEDULER_H_

#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"

namespace blink {
namespace scheduler {

class FakePageScheduler final : public PageScheduler {
 public:
  FakePageScheduler(bool is_audio_playing, bool is_throttling_exempt)
      : is_audio_playing_(is_audio_playing),
        is_throttling_exempt_(is_throttling_exempt),
        agent_group_scheduler_(WebAgentGroupScheduler::CreateForTesting()) {}
  FakePageScheduler(const FakePageScheduler&) = delete;
  FakePageScheduler& operator=(const FakePageScheduler&) = delete;

  class Builder {
   public:
    Builder() = default;
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    Builder& SetIsAudioPlaying(bool is_audio_playing) {
      is_audio_playing_ = is_audio_playing;
      return *this;
    }

    Builder& SetIsThrottlingExempt(bool is_throttling_exempt) {
      is_throttling_exempt_ = is_throttling_exempt;
      return *this;
    }

    std::unique_ptr<FakePageScheduler> Build() {
      return std::make_unique<FakePageScheduler>(is_audio_playing_,
                                                 is_throttling_exempt_);
    }

   private:
    bool is_audio_playing_ = false;
    bool is_throttling_exempt_ = false;
  };

  bool IsAudioPlaying() const override { return is_audio_playing_; }

  bool IsExemptFromBudgetBasedThrottling() const override {
    return is_throttling_exempt_;
  }

  // PageScheduler implementation:
  void OnTitleOrFaviconUpdated() override {}
  void SetPageVisible(bool is_page_visible) override {}
  void SetPageFrozen(bool is_page_frozen) override {}
  void SetPageBackForwardCached(bool) override {}
  bool IsMainFrameLocal() const override { return true; }
  void SetIsMainFrameLocal(bool is_local) override {}
  void OnLocalMainFrameNetworkAlmostIdle() override {}

  std::unique_ptr<FrameScheduler> CreateFrameScheduler(
      FrameScheduler::Delegate* delegate,
      BlameContext* blame_context,
      FrameScheduler::FrameType frame_type) override {
    return nullptr;
  }
  base::TimeTicks EnableVirtualTime(base::Time initial_time) override {
    return base::TimeTicks();
  }
  void DisableVirtualTimeForTesting() override {}
  bool VirtualTimeAllowedToAdvance() const override { return false; }
  void SetVirtualTimePolicy(VirtualTimePolicy policy) override {}
  void GrantVirtualTimeBudget(base::TimeDelta budget,
                              base::OnceClosure callback) override {}
  void SetMaxVirtualTimeTaskStarvationCount(int count) override {}
  void AudioStateChanged(bool is_audio_playing) override {}
  bool OptedOutFromAggressiveThrottlingForTest() const override {
    return false;
  }
  bool RequestBeginMainFrameNotExpected(bool new_state) override {
    return false;
  }
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration) override {
    return WebScopedVirtualTimePauser();
  }
  scheduler::WebAgentGroupScheduler& GetAgentGroupScheduler() override {
    return *agent_group_scheduler_;
  }
  bool IsInBackForwardCache() const override { return false; }

 private:
  bool is_audio_playing_;
  bool is_throttling_exempt_;
  std::unique_ptr<WebAgentGroupScheduler> agent_group_scheduler_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_FAKE_PAGE_SCHEDULER_H_
