// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_VIEW_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_VIEW_SCHEDULER_H_

#include "platform/PlatformExport.h"
#include "platform/WebFrameScheduler.h"
#include "platform/wtf/Functional.h"
#include "public/platform/BlameContext.h"

#include <memory>

namespace blink {

class PLATFORM_EXPORT WebViewScheduler {
 public:
  class PLATFORM_EXPORT WebViewSchedulerDelegate {
   public:
    virtual ~WebViewSchedulerDelegate() {}

    virtual void RequestBeginMainFrameNotExpected(bool new_state) = 0;
    virtual void SetPageStopped(bool) = 0;
  };

  virtual ~WebViewScheduler() {}

  // The scheduler may throttle tasks associated with background pages.
  virtual void SetPageVisible(bool) = 0;
  // The scheduler transitions app to and from STOPPED state in background.
  virtual void SetPageStopped(bool) = 0;

  // Creates a new WebFrameScheduler. The caller is responsible for deleting
  // it. All tasks executed by the frame scheduler will be attributed to
  // |blame_context|.
  virtual std::unique_ptr<WebFrameScheduler> CreateFrameScheduler(
      BlameContext* blame_context,
      WebFrameScheduler::FrameType) = 0;

  // Instructs this WebViewScheduler to use virtual time. When virtual time is
  // enabled the system doesn't actually sleep for the delays between tasks
  // before executing them. Returns the TimeTicks that virtual time offsets will
  // be relative to.
  //
  // E.g: A-E are delayed tasks
  //
  // |    A   B C  D           E  (normal)
  // |-----------------------------> time
  //
  // |ABCDE                       (virtual time)
  // |-----------------------------> time
  virtual base::TimeTicks EnableVirtualTime() = 0;

  // Disables virtual time. Note that this is only used for testing, because
  // there's no reason to do this in production.
  virtual void DisableVirtualTimeForTesting() = 0;

  // Returns true if virtual time is currently allowed to advance.
  virtual bool VirtualTimeAllowedToAdvance() const = 0;

  enum class VirtualTimePolicy {
    // In this policy virtual time is allowed to advance. If the blink scheduler
    // runs out of immediate work, the virtual timebase will be incremented so
    // that the next sceduled timer may fire.  NOTE Tasks will be run in time
    // order (as usual).
    kAdvance,

    // In this policy virtual time is not allowed to advance. Delayed tasks
    // posted to WebTaskRunners owned by any child WebFrameSchedulers will be
    // paused, unless their scheduled run time is less than or equal to the
    // current virtual time.  Note non-delayed tasks will run as normal.
    kPause,

    // In this policy virtual time is allowed to advance unless there are
    // pending network fetches associated any child WebFrameScheduler, or a
    // document is being parsed on a background thread. Initially virtual time
    // is not allowed to advance until we have seen at least one load. The aim
    // being to try and make loading (more) deterministic.
    kDeterministicLoading,
  };

  // Sets the virtual time policy, which is applied imemdiatly to all child
  // WebFrameSchedulers.
  virtual void SetVirtualTimePolicy(VirtualTimePolicy) = 0;

  class PLATFORM_EXPORT VirtualTimeObserver {
   public:
    virtual ~VirtualTimeObserver() {}

    // Called when virtual time advances. |virtual_time_offset| is the offset
    // between the current virtual time and the initial virtual time when
    // EnableVirtualTime() was called.
    virtual void OnVirtualTimeAdvanced(base::TimeDelta virtual_time_offset) = 0;

    // Called when virtual time pauses for any reason. |virtual_time_offset| is
    // the offset between the current virtual time and the initial virtual time
    // when EnableVirtualTime() was called.
    virtual void OnVirtualTimePaused(base::TimeDelta virtual_time_offset) = 0;
  };

  // Adds a VirtualTimeObserver instance to be notified when virtual time has
  // been paused.
  virtual void AddVirtualTimeObserver(VirtualTimeObserver*) = 0;
  virtual void RemoveVirtualTimeObserver(VirtualTimeObserver*) = 0;

  // Set the remaining virtual time budget to |budget|. Once the budget runs
  // out, |budget_exhausted_callback| is called. Note that the virtual time
  // policy is not affected when the budget expires.
  virtual void GrantVirtualTimeBudget(
      base::TimeDelta budget,
      WTF::Closure budget_exhausted_callback) = 0;

  // It's possible for pages to send infinite messages which can arbitrarily
  // block virtual time.  We can prevent this by setting an upper limit on the
  // number of tasks that can run before virtual time is advanced.
  // NB this anti-starvation logic doesn't apply to VirtualTimePolicy::kPause.
  virtual void SetMaxVirtualTimeTaskStarvationCount(
      int max_task_starvation_count) = 0;

  virtual void AudioStateChanged(bool is_audio_playing) = 0;

  virtual bool IsPlayingAudio() const = 0;

  // Returns true if the page should be exempted from aggressive throttling
  // (e.g. due to a page maintaining an active connection).
  virtual bool IsExemptFromBudgetBasedThrottling() const = 0;

  virtual bool HasActiveConnectionForTest() const = 0;

  virtual void RequestBeginMainFrameNotExpected(bool new_state) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_VIEW_SCHEDULER_H_
