/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2009 Adam Barth. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/navigation_scheduler.h"

#include <memory>
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/form_submission.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader_state_machine.h"
#include "third_party/blink/renderer/core/loader/scheduled_navigation.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class ScheduledRedirect final : public ScheduledNavigation {
 public:
  ScheduledRedirect(double delay,
                    Document* origin_document,
                    const KURL& url,
                    Document::HttpRefreshType http_refresh_type,
                    WebFrameLoadType frame_load_type,
                    base::TimeTicks input_timestamp)
      : ScheduledNavigation(ToReason(http_refresh_type),
                            delay,
                            origin_document,
                            url,
                            frame_load_type,
                            input_timestamp) {
    ClearUserGesture();
  }

  bool ShouldStartTimer(LocalFrame* frame) override {
    return frame->GetDocument()->LoadEventFinished();
  }

  void Fire(LocalFrame* frame) override {
    std::unique_ptr<UserGestureIndicator> gesture_indicator =
        CreateUserGestureIndicator();
    FrameLoadRequest request(OriginDocument(), ResourceRequest(Url()), "_self");
    request.SetInputStartTime(InputTimestamp());
    WebFrameLoadType load_type = LoadType();
    if (EqualIgnoringFragmentIdentifier(frame->GetDocument()->Url(),
                                        request.GetResourceRequest().Url())) {
      request.GetResourceRequest().SetCacheMode(
          mojom::FetchCacheMode::kValidateCache);
      load_type = WebFrameLoadType::kReload;
    }
    request.SetClientRedirectReason(GetReason());
    frame->Loader().StartNavigation(request, load_type);
  }

 private:
  static ClientNavigationReason ToReason(
      Document::HttpRefreshType http_refresh_type) {
    switch (http_refresh_type) {
      case Document::HttpRefreshType::kHttpRefreshFromHeader:
        return ClientNavigationReason::kHttpHeaderRefresh;
      case Document::HttpRefreshType::kHttpRefreshFromMetaTag:
        return ClientNavigationReason::kMetaTagRefresh;
      default:
        break;
    }
    NOTREACHED();
    return ClientNavigationReason::kMetaTagRefresh;
  }
};

NavigationScheduler::NavigationScheduler(LocalFrame* frame) : frame_(frame) {}

NavigationScheduler::~NavigationScheduler() {
}

bool NavigationScheduler::IsNavigationScheduledWithin(double interval) const {
  return redirect_ && redirect_->Delay() <= interval;
}

inline bool NavigationScheduler::ShouldScheduleNavigation(
    const KURL& url) const {
  return frame_->GetPage() && frame_->IsNavigationAllowed();
}

void NavigationScheduler::ScheduleRedirect(
    double delay,
    const KURL& url,
    Document::HttpRefreshType http_refresh_type) {
  if (!ShouldScheduleNavigation(url))
    return;
  if (delay < 0 || delay > INT_MAX / 1000)
    return;
  if (url.IsEmpty())
    return;

  // We want a new back/forward list item if the refresh timeout is > 1 second.
  if (!redirect_ || delay <= redirect_->Delay()) {
    WebFrameLoadType frame_load_type = WebFrameLoadType::kStandard;
    if (delay <= 1)
      frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
    Schedule(MakeGarbageCollected<ScheduledRedirect>(
        delay, frame_->GetDocument(), url, http_refresh_type, frame_load_type,
        InputTimestamp()));
  }
}

base::TimeTicks NavigationScheduler::InputTimestamp() {
  if (const WebInputEvent* input_event = CurrentInputEvent::Get()) {
    return input_event->TimeStamp();
  }
  return base::TimeTicks();
}

void NavigationScheduler::NavigateTask() {
  if (!frame_->GetPage())
    return;
  if (frame_->GetPage()->Paused()) {
    probe::FrameClearedScheduledNavigation(frame_);
    return;
  }

  ScheduledNavigation* redirect(redirect_.Release());
  redirect->Fire(frame_);
  probe::FrameClearedScheduledNavigation(frame_);
}

void NavigationScheduler::Schedule(ScheduledNavigation* redirect) {
  DCHECK(frame_->GetPage());

  // In a back/forward navigation, we sometimes restore history state to
  // iframes, even though the state was generated dynamically and JS will try to
  // put something different in the iframe. In this case, we will load stale
  // things and/or confuse the JS when it shortly thereafter tries to schedule a
  // location change. Let the JS have its way.
  // FIXME: This check seems out of place.
  if (!frame_->Loader().StateMachine()->CommittedFirstRealDocumentLoad() &&
      frame_->Loader().GetProvisionalDocumentLoader() &&
      frame_->Loader().GetProvisionalDocumentLoader()->DidStart()) {
    frame_->Loader().StopAllLoaders();
    if (!frame_->GetPage())
      return;
  }

  Cancel();
  redirect_ = redirect;
  StartTimer();
}

void NavigationScheduler::StartTimer() {
  if (!redirect_)
    return;

  DCHECK(frame_->GetPage());
  if (navigate_task_handle_.IsActive())
    return;
  if (!redirect_->ShouldStartTimer(frame_))
    return;

  // wrapWeakPersistent(this) is safe because a posted task is canceled when the
  // task handle is destroyed on the dtor of this NavigationScheduler.
  navigate_task_handle_ = PostDelayedCancellableTask(
      *frame_->GetFrameScheduler()->GetTaskRunner(TaskType::kInternalLoading),
      FROM_HERE,
      WTF::Bind(&NavigationScheduler::NavigateTask, WrapWeakPersistent(this)),
      TimeDelta::FromSecondsD(redirect_->Delay()));

  probe::FrameScheduledNavigation(frame_, redirect_->Url(), redirect_->Delay(),
                                  redirect_->GetReason());
}

void NavigationScheduler::Cancel() {
  if (navigate_task_handle_.IsActive()) {
    probe::FrameClearedScheduledNavigation(frame_);
  }
  if (frame_->GetDocument())
    frame_->GetDocument()->CancelPendingJavaScriptUrls();
  navigate_task_handle_.Cancel();
  redirect_.Clear();
}

void NavigationScheduler::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(redirect_);
}

}  // namespace blink
