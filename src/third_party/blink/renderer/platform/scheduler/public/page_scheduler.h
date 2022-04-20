// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_PAGE_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_PAGE_SCHEDULER_H_

#include <memory>
#include "third_party/blink/public/platform/blame_context.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_lifecycle_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/scheduler/public/virtual_time_controller.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace scheduler {
class WebAgentGroupScheduler;
}  // namespace scheduler

class PLATFORM_EXPORT PageScheduler {
 public:
  class PLATFORM_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    // An "ordinary" page is a fully-featured page owned by a web view.
    virtual bool IsOrdinary() const = 0;
    virtual void ReportIntervention(const WTF::String& message) = 0;
    // Returns true if the request has been succcessfully relayed to the
    // compositor.
    virtual bool RequestBeginMainFrameNotExpected(bool new_state) = 0;
    virtual void OnSetPageFrozen(bool is_frozen) = 0;
    // Returns true iff the network is idle for the local main frame.
    // Always returns false if the main frame is remote.
    virtual bool LocalMainFrameNetworkIsAlmostIdle() const { return true; }
  };

  virtual ~PageScheduler() = default;

  // Signals that communications with the user took place via either a title
  // updates or a change to the favicon.
  virtual void OnTitleOrFaviconUpdated() = 0;
  // The scheduler may throttle tasks associated with background pages.
  virtual void SetPageVisible(bool) = 0;
  // The scheduler transitions app to and from FROZEN state in background.
  virtual void SetPageFrozen(bool) = 0;
  // Handles operations required for storing the page in the back-forward cache.
  virtual void SetPageBackForwardCached(bool) = 0;
  // Whether the main frame of this page is local or not (remote).
  virtual bool IsMainFrameLocal() const = 0;
  virtual void SetIsMainFrameLocal(bool) = 0;
  // Invoked when the local main frame's network becomes almost idle.
  // Never invoked if the main frame is remote.
  virtual void OnLocalMainFrameNetworkAlmostIdle() = 0;
  // Whether the main frame of this page is in BackForwardCache or not.
  virtual bool IsInBackForwardCache() const = 0;

  // Creates a new FrameScheduler. The caller is responsible for deleting
  // it. All tasks executed by the frame scheduler will be attributed to
  // |blame_context|.
  virtual std::unique_ptr<FrameScheduler> CreateFrameScheduler(
      FrameScheduler::Delegate* delegate,
      BlameContext*,
      FrameScheduler::FrameType) = 0;

  virtual void AudioStateChanged(bool is_audio_playing) = 0;

  virtual bool IsAudioPlaying() const = 0;

  // Returns true if the page should be exempted from aggressive throttling
  // (e.g. due to a page maintaining an active connection).
  virtual bool IsExemptFromBudgetBasedThrottling() const = 0;

  virtual bool OptedOutFromAggressiveThrottlingForTest() const = 0;

  // Returns true if the request has been succcessfully relayed to the
  // compositor.
  virtual bool RequestBeginMainFrameNotExpected(bool new_state) = 0;

  // Returns WebAgentGroupScheduler
  virtual scheduler::WebAgentGroupScheduler& GetAgentGroupScheduler() = 0;

  // Guaranteed to be non-null for real PageScheduler implementation, but may
  // be null in unit tests.
  virtual VirtualTimeController* GetVirtualTimeController() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_PAGE_SCHEDULER_H_
