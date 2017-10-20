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

#include "core/loader/NavigationScheduler.h"

#include <memory>
#include "bindings/core/v8/ScriptController.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/dom/events/Event.h"
#include "core/frame/Deprecation.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/forms/HTMLFormElement.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FormSubmission.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderStateMachine.h"
#include "core/loader/ScheduledNavigation.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "platform/Histogram.h"
#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"

namespace blink {

namespace {

// Add new scheduled navigation types before ScheduledLastEntry
enum ScheduledNavigationType {
  kScheduledReload,
  kScheduledFormSubmission,
  kScheduledURLNavigation,
  kScheduledRedirect,
  kScheduledFrameNavigation,
  kScheduledPageBlock,

  kScheduledLastEntry
};

// If the current frame has a provisional document loader, a scheduled
// navigation might abort that load. Log those occurrences until
// crbug.com/557430 is resolved.
void MaybeLogScheduledNavigationClobber(ScheduledNavigationType type,
                                        LocalFrame* frame) {
  if (!frame->Loader().GetProvisionalDocumentLoader())
    return;
  // Include enumeration values userGesture variants.
  DEFINE_STATIC_LOCAL(EnumerationHistogram,
                      scheduled_navigation_clobber_histogram,
                      ("Navigation.Scheduled.MaybeCausedAbort",
                       ScheduledNavigationType::kScheduledLastEntry * 2));

  int value = UserGestureIndicator::ProcessingUserGesture()
                  ? type + kScheduledLastEntry
                  : type;
  scheduled_navigation_clobber_histogram.Count(value);

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, scheduled_clobber_abort_time_histogram,
      ("Navigation.Scheduled.MaybeCausedAbort.Time", 1, 10000, 50));
  double navigation_start = frame->Loader()
                                .GetProvisionalDocumentLoader()
                                ->GetTiming()
                                .NavigationStart();
  if (navigation_start) {
    scheduled_clobber_abort_time_histogram.Count(MonotonicallyIncreasingTime() -
                                                 navigation_start);
  }
}

}  // namespace

unsigned NavigationDisablerForBeforeUnload::navigation_disable_count_ = 0;

class ScheduledURLNavigation : public ScheduledNavigation {
 protected:
  ScheduledURLNavigation(Reason reason,
                         double delay,
                         Document* origin_document,
                         const KURL& url,
                         bool replaces_current_item,
                         bool is_location_change)
      : ScheduledNavigation(reason,
                            delay,
                            origin_document,
                            replaces_current_item,
                            is_location_change),
        url_(url),
        should_check_main_world_content_security_policy_(
            kCheckContentSecurityPolicy) {
    if (ContentSecurityPolicy::ShouldBypassMainWorld(origin_document)) {
      should_check_main_world_content_security_policy_ =
          kDoNotCheckContentSecurityPolicy;
    }
  }

  void Fire(LocalFrame* frame) override {
    std::unique_ptr<UserGestureIndicator> gesture_indicator =
        CreateUserGestureIndicator();
    FrameLoadRequest request(OriginDocument(), ResourceRequest(url_), "_self",
                             should_check_main_world_content_security_policy_);
    request.SetReplacesCurrentItem(ReplacesCurrentItem());
    request.SetClientRedirect(ClientRedirectPolicy::kClientRedirect);

    ScheduledNavigationType type =
        IsLocationChange() ? ScheduledNavigationType::kScheduledFrameNavigation
                           : ScheduledNavigationType::kScheduledURLNavigation;
    MaybeLogScheduledNavigationClobber(type, frame);
    frame->Loader().Load(request);
  }

  KURL Url() const override { return url_; }

 private:
  KURL url_;
  ContentSecurityPolicyDisposition
      should_check_main_world_content_security_policy_;
};

class ScheduledRedirect final : public ScheduledURLNavigation {
 public:
  static ScheduledRedirect* Create(double delay,
                                   Document* origin_document,
                                   const KURL& url,
                                   Document::HttpRefreshType http_refresh_type,
                                   bool replaces_current_item) {
    return new ScheduledRedirect(delay, origin_document, url, http_refresh_type,
                                 replaces_current_item);
  }

  bool ShouldStartTimer(LocalFrame* frame) override {
    return frame->GetDocument()->LoadEventFinished();
  }

  void Fire(LocalFrame* frame) override {
    std::unique_ptr<UserGestureIndicator> gesture_indicator =
        CreateUserGestureIndicator();
    FrameLoadRequest request(OriginDocument(), ResourceRequest(Url()), "_self");
    request.SetReplacesCurrentItem(ReplacesCurrentItem());
    if (EqualIgnoringFragmentIdentifier(frame->GetDocument()->Url(),
                                        request.GetResourceRequest().Url())) {
      request.GetResourceRequest().SetCachePolicy(
          WebCachePolicy::kValidatingCacheData);
    }
    request.SetClientRedirect(ClientRedirectPolicy::kClientRedirect);
    MaybeLogScheduledNavigationClobber(
        ScheduledNavigationType::kScheduledRedirect, frame);
    frame->Loader().Load(request);
  }

 private:
  static Reason ToReason(Document::HttpRefreshType http_refresh_type) {
    switch (http_refresh_type) {
      case Document::HttpRefreshType::kHttpRefreshFromHeader:
        return Reason::kHttpHeaderRefresh;
      case Document::HttpRefreshType::kHttpRefreshFromMetaTag:
        return Reason::kMetaTagRefresh;
      default:
        break;
    }
    NOTREACHED();
    return Reason::kMetaTagRefresh;
  }

  ScheduledRedirect(double delay,
                    Document* origin_document,
                    const KURL& url,
                    Document::HttpRefreshType http_refresh_type,
                    bool replaces_current_item)
      : ScheduledURLNavigation(ToReason(http_refresh_type),
                               delay,
                               origin_document,
                               url,
                               replaces_current_item,
                               false) {
    ClearUserGesture();
  }
};

class ScheduledFrameNavigation final : public ScheduledURLNavigation {
 public:
  static ScheduledFrameNavigation* Create(Document* origin_document,
                                          const KURL& url,
                                          bool replaces_current_item) {
    return new ScheduledFrameNavigation(origin_document, url,
                                        replaces_current_item);
  }

 private:
  ScheduledFrameNavigation(Document* origin_document,
                           const KURL& url,
                           bool replaces_current_item)
      : ScheduledURLNavigation(Reason::kFrameNavigation,
                               0.0,
                               origin_document,
                               url,
                               replaces_current_item,
                               !url.ProtocolIsJavaScript()) {}
};

class ScheduledReload final : public ScheduledNavigation {
 public:
  static ScheduledReload* Create(LocalFrame* frame) {
    return new ScheduledReload(frame);
  }

  void Fire(LocalFrame* frame) override {
    std::unique_ptr<UserGestureIndicator> gesture_indicator =
        CreateUserGestureIndicator();
    ResourceRequest resource_request = frame->Loader().ResourceRequestForReload(
        kFrameLoadTypeReload, KURL(), ClientRedirectPolicy::kClientRedirect);
    if (resource_request.IsNull())
      return;
    FrameLoadRequest request = FrameLoadRequest(nullptr, resource_request);
    request.SetClientRedirect(ClientRedirectPolicy::kClientRedirect);
    MaybeLogScheduledNavigationClobber(
        ScheduledNavigationType::kScheduledReload, frame);
    frame->Loader().Load(request, kFrameLoadTypeReload);
  }

  KURL Url() const override { return frame_->GetDocument()->Url(); }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(frame_);
    ScheduledNavigation::Trace(visitor);
  }

 private:
  explicit ScheduledReload(LocalFrame* frame)
      : ScheduledNavigation(Reason::kReload, 0.0, nullptr, true, true),
        frame_(frame) {}

  Member<LocalFrame> frame_;
};

class ScheduledPageBlock final : public ScheduledNavigation {
 public:
  static ScheduledPageBlock* Create(Document* origin_document, int reason) {
    return new ScheduledPageBlock(origin_document, reason);
  }

  void Fire(LocalFrame* frame) override {
    frame->Client()->LoadErrorPage(reason_);
  }

  KURL Url() const override { return KURL(); }

 private:
  ScheduledPageBlock(Document* origin_document, int reason)
      : ScheduledNavigation(Reason::kPageBlock,
                            0.0,
                            origin_document,
                            true,
                            true),
        reason_(reason) {}

  int reason_;
};

class ScheduledFormSubmission final : public ScheduledNavigation {
 public:
  static ScheduledFormSubmission* Create(Document* document,
                                         FormSubmission* submission,
                                         bool replaces_current_item) {
    return new ScheduledFormSubmission(document, submission,
                                       replaces_current_item);
  }

  void Fire(LocalFrame* frame) override {
    std::unique_ptr<UserGestureIndicator> gesture_indicator =
        CreateUserGestureIndicator();
    FrameLoadRequest frame_request =
        submission_->CreateFrameLoadRequest(OriginDocument());
    frame_request.SetReplacesCurrentItem(ReplacesCurrentItem());
    MaybeLogScheduledNavigationClobber(
        ScheduledNavigationType::kScheduledFormSubmission, frame);
    frame->Loader().Load(frame_request);
  }

  KURL Url() const override { return submission_->RequestURL(); }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(submission_);
    ScheduledNavigation::Trace(visitor);
  }

 private:
  ScheduledFormSubmission(Document* document,
                          FormSubmission* submission,
                          bool replaces_current_item)
      : ScheduledNavigation(submission->Method() == FormSubmission::kGetMethod
                                ? Reason::kFormSubmissionGet
                                : Reason::kFormSubmissionPost,
                            0,
                            document,
                            replaces_current_item,
                            true),
        submission_(submission) {
    DCHECK_NE(submission->Method(), FormSubmission::kDialogMethod);
    DCHECK(submission_->Form());
  }

  Member<FormSubmission> submission_;
};

NavigationScheduler::NavigationScheduler(LocalFrame* frame)
    : frame_(frame),
      frame_type_(
          frame_->IsMainFrame()
              ? scheduler::RendererScheduler::NavigatingFrameType::kMainFrame
              : scheduler::RendererScheduler::NavigatingFrameType::
                    kChildFrame) {}

NavigationScheduler::~NavigationScheduler() {
  if (navigate_task_handle_.IsActive()) {
    Platform::Current()->CurrentThread()->Scheduler()->RemovePendingNavigation(
        frame_type_);
  }
}

bool NavigationScheduler::LocationChangePending() {
  return redirect_ && redirect_->IsLocationChange();
}

bool NavigationScheduler::IsNavigationScheduledWithin(double interval) const {
  return redirect_ && redirect_->Delay() <= interval;
}

// TODO(dcheng): There are really two different load blocking concepts at work
// here and they have been incorrectly tangled together.
//
// 1. NavigationDisablerForBeforeUnload is for blocking navigation scheduling
//    during a beforeunload events. Scheduled navigations during beforeunload
//    would make it possible to get trapped in an endless loop of beforeunload
//    dialogs.
//
//    Checking Frame::isNavigationAllowed() doesn't make sense in this context:
//    NavigationScheduler is always cleared when a new load commits, so it's
//    impossible for a scheduled navigation to clobber a navigation that just
//    committed.
//
// 2. FrameNavigationDisabler / LocalFrame::isNavigationAllowed() are intended
//    to prevent Documents from being reattached during destruction, since it
//    can cause bugs with security origin confusion. This is primarily intended
//    to block /synchronous/ navigations during things lke
//    Document::detachLayoutTree().
inline bool NavigationScheduler::ShouldScheduleReload() const {
  return frame_->GetPage() && frame_->IsNavigationAllowed() &&
         NavigationDisablerForBeforeUnload::IsNavigationAllowed();
}

inline bool NavigationScheduler::ShouldScheduleNavigation(
    const KURL& url) const {
  return frame_->GetPage() && frame_->IsNavigationAllowed() &&
         (url.ProtocolIsJavaScript() ||
          NavigationDisablerForBeforeUnload::IsNavigationAllowed());
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
    Schedule(ScheduledRedirect::Create(delay, frame_->GetDocument(), url,
                                       http_refresh_type, delay <= 1));
  }
}

bool NavigationScheduler::MustReplaceCurrentItem(LocalFrame* target_frame) {
  // Non-user navigation before the page has finished firing onload should not
  // create a new back/forward item. See https://webkit.org/b/42861 for the
  // original motivation for this.
  if (!target_frame->GetDocument()->LoadEventFinished() &&
      !UserGestureIndicator::ProcessingUserGesture())
    return true;

  // Navigation of a subframe during loading of an ancestor frame does not
  // create a new back/forward item. The definition of "during load" is any time
  // before all handlers for the load event have been run. See
  // https://bugs.webkit.org/show_bug.cgi?id=14957 for the original motivation
  // for this.
  Frame* parent_frame = target_frame->Tree().Parent();
  return parent_frame && parent_frame->IsLocalFrame() &&
         !ToLocalFrame(parent_frame)->Loader().AllAncestorsAreComplete();
}

void NavigationScheduler::ScheduleFrameNavigation(Document* origin_document,
                                                  const KURL& url,
                                                  bool replaces_current_item) {
  if (!ShouldScheduleNavigation(url))
    return;

  replaces_current_item =
      replaces_current_item || MustReplaceCurrentItem(frame_);

  // If the URL we're going to navigate to is the same as the current one,
  // except for the fragment part, we don't need to schedule the location
  // change. We'll skip this optimization for cross-origin navigations to
  // minimize the navigator's ability to execute timing attacks.
  if (origin_document->GetSecurityOrigin()->CanAccess(
          frame_->GetDocument()->GetSecurityOrigin())) {
    if (url.HasFragmentIdentifier() &&
        EqualIgnoringFragmentIdentifier(frame_->GetDocument()->Url(), url)) {
      FrameLoadRequest request(origin_document, ResourceRequest(url), "_self");
      request.SetReplacesCurrentItem(replaces_current_item);
      if (replaces_current_item)
        request.SetClientRedirect(ClientRedirectPolicy::kClientRedirect);
      frame_->Loader().Load(request);
      return;
    }
  }

  Schedule(ScheduledFrameNavigation::Create(origin_document, url,
                                            replaces_current_item));
}

void NavigationScheduler::SchedulePageBlock(Document* origin_document,
                                            int reason) {
  DCHECK(frame_->GetPage());
  Schedule(ScheduledPageBlock::Create(origin_document, reason));
}

void NavigationScheduler::ScheduleFormSubmission(Document* document,
                                                 FormSubmission* submission) {
  DCHECK(frame_->GetPage());
  Schedule(ScheduledFormSubmission::Create(document, submission,
                                           MustReplaceCurrentItem(frame_)));
}

void NavigationScheduler::ScheduleReload() {
  if (!ShouldScheduleReload())
    return;
  if (frame_->GetDocument()->Url().IsEmpty())
    return;
  Schedule(ScheduledReload::Create(frame_));
}

void NavigationScheduler::NavigateTask() {
  Platform::Current()->CurrentThread()->Scheduler()->RemovePendingNavigation(
      frame_type_);

  if (!frame_->GetPage())
    return;
  if (frame_->GetPage()->Paused()) {
    probe::frameClearedScheduledNavigation(frame_);
    return;
  }

  ScheduledNavigation* redirect(redirect_.Release());
  redirect->Fire(frame_);
  probe::frameClearedScheduledNavigation(frame_);
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
  if (redirect_->IsLocationChange())
    frame_->GetDocument()->SuppressLoadEvent();
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

  WebScheduler* scheduler = Platform::Current()->CurrentThread()->Scheduler();
  scheduler->AddPendingNavigation(frame_type_);

  // wrapWeakPersistent(this) is safe because a posted task is canceled when the
  // task handle is destroyed on the dtor of this NavigationScheduler.
  navigate_task_handle_ = frame_->FrameScheduler()
                              ->GetTaskRunner(TaskType::kUnspecedLoading)
                              ->PostDelayedCancellableTask(
                                  BLINK_FROM_HERE,
                                  WTF::Bind(&NavigationScheduler::NavigateTask,
                                            WrapWeakPersistent(this)),
                                  TimeDelta::FromSecondsD(redirect_->Delay()));

  probe::frameScheduledNavigation(frame_, redirect_.Get());
}

void NavigationScheduler::Cancel() {
  if (navigate_task_handle_.IsActive()) {
    Platform::Current()->CurrentThread()->Scheduler()->RemovePendingNavigation(
        frame_type_);
    probe::frameClearedScheduledNavigation(frame_);
  }
  navigate_task_handle_.Cancel();
  redirect_.Clear();
}

void NavigationScheduler::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(redirect_);
}

}  // namespace blink
