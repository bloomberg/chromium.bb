// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_PAGE_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_PAGE_SCHEDULER_H_

#include "platform/scheduler/public/page_scheduler.h"

namespace blink {
namespace scheduler {

class FakePageScheduler final : public PageScheduler {
 public:
  FakePageScheduler(bool is_playing_audio, bool is_throttling_exempt)
      : is_playing_audio_(is_playing_audio),
        is_throttling_exempt_(is_throttling_exempt) {}

  class Builder {
   public:
    Builder() = default;

    Builder& SetIsPlayingAudio(bool is_playing_audio) {
      is_playing_audio_ = is_playing_audio;
      return *this;
    }

    Builder& SetIsThrottlingExempt(bool is_throttling_exempt) {
      is_throttling_exempt_ = is_throttling_exempt;
      return *this;
    }

    std::unique_ptr<FakePageScheduler> Build() {
      return std::make_unique<FakePageScheduler>(is_playing_audio_,
                                                 is_throttling_exempt_);
    }

   private:
    bool is_playing_audio_ = false;
    bool is_throttling_exempt_ = false;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  bool IsPlayingAudio() const override { return is_playing_audio_; }

  bool IsExemptFromBudgetBasedThrottling() const override {
    return is_throttling_exempt_;
  }

  void SetPageVisible(bool is_page_visible) override {}
  void SetPageFrozen(bool is_page_frozen) override {}

  std::unique_ptr<FrameScheduler> CreateFrameScheduler(
      BlameContext* blame_context,
      FrameScheduler::FrameType frame_type) override {
    return nullptr;
  }
  base::TimeTicks EnableVirtualTime() override { return base::TimeTicks(); }
  void DisableVirtualTimeForTesting() override {}
  bool VirtualTimeAllowedToAdvance() const override { return false; }
  void SetVirtualTimePolicy(VirtualTimePolicy policy) override {}
  void AddVirtualTimeObserver(VirtualTimeObserver* observer) override {}
  void RemoveVirtualTimeObserver(VirtualTimeObserver* observer) override {}
  void SetInitialVirtualTimeOffset(base::TimeDelta offset) override {}
  void GrantVirtualTimeBudget(base::TimeDelta budget,
                              base::OnceClosure callback) override {}
  void SetMaxVirtualTimeTaskStarvationCount(int count) override {}
  void AudioStateChanged(bool is_audio_playing) override {}
  bool HasActiveConnectionForTest() const override { return false; }
  void RequestBeginMainFrameNotExpected(bool new_state) override {}

 private:
  bool is_playing_audio_;
  bool is_throttling_exempt_;

  DISALLOW_COPY_AND_ASSIGN(FakePageScheduler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_PAGE_SCHEDULER_H_
