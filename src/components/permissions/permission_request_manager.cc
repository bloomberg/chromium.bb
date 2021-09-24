// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_manager.h"

#include <algorithm>
#include <string>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/autofill_assistant/browser/public/runtime_manager.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/request_type.h"
#include "components/permissions/switches.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace permissions {

const char kAbusiveNotificationRequestsEnforcementMessage[] =
    "Chrome is blocking notification permission requests on this site because "
    "the site tends to show permission requests that mislead, trick, or force "
    "users into allowing notifications. You should fix the issues as soon as "
    "possible and submit your site for another review. Learn more at "
    "https://support.google.com/webtools/answer/9799048.";

const char kAbusiveNotificationRequestsWarningMessage[] =
    "Chrome might start blocking notification permission requests on this site "
    "in the future because the site tends to show permission requests that "
    "mislead, trick, or force users into allowing notifications. You should "
    "fix the issues as soon as possible and submit your site for another "
    "review. Learn more at https://support.google.com/webtools/answer/9799048.";

constexpr char kAbusiveNotificationContentEnforcementMessage[] =
    "Chrome is blocking notification permission requests on this site because "
    "the site tends to show notifications with content that mislead or trick "
    "users. You should fix the issues as soon as possible and submit your site "
    "for another review. Learn more at "
    "https://support.google.com/webtools/answer/9799048";

constexpr char kAbusiveNotificationContentWarningMessage[] =
    "Chrome might start blocking notification permission requests on this site "
    "in the future because the site tends to show notifications with content "
    "that mislead or trick users. You should fix the issues as soon as "
    "possible and submit your site for another review. Learn more at "
    "https://support.google.com/webtools/answer/9799048";

namespace {

// When there are multiple permissions requests in `queued_requests_`, we try to
// reorder them based on the acceptance rates. Notifications and Geolocations
// have one of the lowest acceptance, hence they have the lowest priority and
// will be shown the last.
bool IsLowPriorityRequest(PermissionRequest* request) {
  return request->request_type() == RequestType::kNotifications ||
         request->request_type() == RequestType::kGeolocation;
}

// In case of multiple permission requests that use chip UI, a newly added
// request will preempt the currently showing request, which is put back to the
// queue, and will be shown later. To reduce user annoyance, if a quiet chip
// permission prompt was displayed longer than `kQuietChipIgnoreTimeout`, we
// consider it as shown long enough and it will not be shown again after it is
// preempted.
// TODO(crbug.com/1221083): If a user switched tabs, do not include that time as
// "shown".
bool ShouldShowQuietRequestAgainIfPreempted(
    absl::optional<base::Time> request_display_start_time) {
  if (request_display_start_time->is_null()) {
    return true;
  }

  static constexpr base::TimeDelta kQuietChipIgnoreTimeout =
      base::TimeDelta::FromSecondsD(8.5);
  return base::Time::Now() - request_display_start_time.value() <
         kQuietChipIgnoreTimeout;
}

bool IsMediaRequest(RequestType type) {
#if !defined(OS_ANDROID)
  if (type == RequestType::kCameraPanTiltZoom)
    return true;
#endif
  return type == RequestType::kMicStream || type == RequestType::kCameraStream;
}

bool IsArOrCameraRequest(RequestType type) {
  return type == RequestType::kArSession || type == RequestType::kCameraStream;
}

bool ShouldGroupRequests(PermissionRequest* a, PermissionRequest* b) {
  if (a->requesting_origin() != b->requesting_origin())
    return false;

  // Group if both requests are media requests.
  if (IsMediaRequest(a->request_type()) && IsMediaRequest(b->request_type())) {
    return true;
  }

  // Group if the requests are an AR and a Camera Access request.
  if (IsArOrCameraRequest(a->request_type()) &&
      IsArOrCameraRequest(b->request_type())) {
    return true;
  }

  return false;
}

}  // namespace

// PermissionRequestManager ----------------------------------------------------

bool PermissionRequestManager::PermissionRequestSource::
    IsSourceFrameInactiveAndDisallowActivation() const {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  return !rfh || rfh->IsInactiveAndDisallowActivation();
}

PermissionRequestManager::~PermissionRequestManager() {
  DCHECK(!IsRequestInProgress());
  DCHECK(duplicate_requests_.empty());
  DCHECK(queued_requests_.empty());
}

void PermissionRequestManager::AddRequest(
    content::RenderFrameHost* source_frame,
    PermissionRequest* request) {
  DCHECK(source_frame);
  DCHECK_EQ(content::WebContents::FromRenderFrameHost(source_frame),
            web_contents());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDenyPermissionPrompts)) {
    request->PermissionDenied();
    request->RequestFinished();
    return;
  }

  if (source_frame->IsInactiveAndDisallowActivation()) {
    request->Cancelled();
    request->RequestFinished();
    return;
  }

  if (is_notification_prompt_cooldown_active_ &&
      request->GetContentSettingsType() == ContentSettingsType::NOTIFICATIONS) {
    // Short-circuit by canceling rather than denying to avoid creating a large
    // number of content setting exceptions on Desktop / disabled notification
    // channels on Android.
    request->Cancelled();
    request->RequestFinished();
    return;
  }

  if (!web_contents_supports_permission_requests_) {
    request->Cancelled();
    request->RequestFinished();
    return;
  }

  // TODO(tsergeant): change the UMA to no longer mention bubbles.
  base::RecordAction(base::UserMetricsAction("PermissionBubbleRequest"));

  // TODO(gbillock): is there a race between an early request on a
  // newly-navigated page and the to-be-cleaned-up requests on the previous
  // page? We should maybe listen to DidStartNavigationToPendingEntry (and
  // any other renderer-side nav initiations?). Double-check this for
  // correct behavior on interstitials -- we probably want to basically queue
  // any request for which GetVisibleURL != GetLastCommittedURL.
  CHECK_EQ(source_frame->GetMainFrame(), web_contents()->GetMainFrame());
  const GURL& main_frame_origin =
      PermissionUtil::GetLastCommittedOriginAsURL(web_contents());
  bool is_main_frame =
      url::Origin::Create(main_frame_origin)
          .IsSameOriginWith(url::Origin::Create(request->requesting_origin()));

  absl::optional<url::Origin> auto_approval_origin =
      PermissionsClient::Get()->GetAutoApprovalOrigin();
  if (auto_approval_origin) {
    if (url::Origin::Create(request->requesting_origin()) ==
        auto_approval_origin.value()) {
      request->PermissionGranted(/*is_one_time=*/false);
    }
    request->RequestFinished();
    return;
  }

  // Cancel permission requests wile Autofill Assistant's UI is shown.
  auto* assistant_runtime_manager =
      autofill_assistant::RuntimeManager::GetForWebContents(web_contents());
  if (assistant_runtime_manager && assistant_runtime_manager->GetState() ==
                                       autofill_assistant::UIState::kShown) {
    request->Cancelled();
    request->RequestFinished();
    return;
  }

  // Don't re-add an existing request or one with a duplicate text request.
  PermissionRequest* existing_request = GetExistingRequest(request);
  if (existing_request) {
    // |request| is a duplicate. Add it to |duplicate_requests_| unless it's the
    // same object as |existing_request| or an existing duplicate.
    if (request == existing_request)
      return;
    auto range = duplicate_requests_.equal_range(existing_request);
    for (auto it = range.first; it != range.second; ++it) {
      if (request == it->second)
        return;
    }
    duplicate_requests_.insert(std::make_pair(existing_request, request));
    return;
  }

  if (is_main_frame) {
    if (IsRequestInProgress()) {
      base::RecordAction(
          base::UserMetricsAction("PermissionBubbleRequestQueued"));
    }
  } else {
    base::RecordAction(
        base::UserMetricsAction("PermissionBubbleIFrameRequestQueued"));
  }

  CurrentRequestFate current_request_fate =
      GetCurrentRequestFateInFaceOfNewRequest(request);

  if (current_request_fate == CurrentRequestFate::Preempt) {
    PreemptAndRequeueCurrentRequest();
  }

  QueueRequest(source_frame, request);

  if (current_request_fate == CurrentRequestFate::Finalize) {
    // FinalizeCurrentRequests will call ScheduleDequeueRequest on its own.
    FinalizeCurrentRequests(PermissionAction::IGNORED);
  } else {
    ScheduleDequeueRequestIfNeeded();
  }
}

PermissionRequestManager::CurrentRequestFate
PermissionRequestManager::GetCurrentRequestFateInFaceOfNewRequest(
    PermissionRequest* new_request) {
  if (base::FeatureList::IsEnabled(features::kPermissionQuietChip)) {
    if (ShouldCurrentRequestUseQuietUI() &&
        !ShouldShowQuietRequestAgainIfPreempted(
            current_request_first_display_time_)) {
      return CurrentRequestFate::Finalize;
    }

    if (base::FeatureList::IsEnabled(features::kPermissionChip)) {
      // Preempt current request if it is a quiet UI request or it is not
      // Notifications or Geolocation.
      if (ShouldCurrentRequestUseQuietUI() ||
          !IsLowPriorityRequest(new_request)) {
        return CurrentRequestFate::Preempt;
      }
    } else {
      if (ShouldCurrentRequestUseQuietUI()) {
        return CurrentRequestFate::Preempt;
      }
    }
  } else {
    if (base::FeatureList::IsEnabled(features::kPermissionChip)) {
      return CurrentRequestFate::Preempt;
    } else if (ShouldCurrentRequestUseQuietUI()) {
      // If we're displaying a quiet permission request, ignore it in favor of a
      // new permission request.
      return CurrentRequestFate::Finalize;
    }
  }

  return CurrentRequestFate::KeepCurrent;
}

void PermissionRequestManager::QueueRequest(
    content::RenderFrameHost* source_frame,
    PermissionRequest* request) {
  PushQueuedRequest(request);
  request_sources_map_.emplace(
      request, PermissionRequestSource({source_frame->GetProcess()->GetID(),
                                        source_frame->GetRoutingID()}));
}

void PermissionRequestManager::PreemptAndRequeueCurrentRequest() {
  ResetViewStateForCurrentRequest();
  for (auto* current_request : requests_) {
    PushQueuedRequest(current_request);
  }
  requests_.clear();
}

void PermissionRequestManager::UpdateAnchor() {
  if (view_) {
    // When the prompt's anchor is being updated, the prompt view can be
    // recreated for the new browser. Because of that, ignore prompt callbacks
    // while doing that.
    base::AutoReset<bool> ignore(&ignore_callbacks_from_prompt_, true);
    view_->UpdateAnchor();
  }
}

void PermissionRequestManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Cooldown lasts until the next user-initiated navigation, which is defined
  // as either a renderer-initiated navigation with a user gesture, or a
  // browser-initiated navigation.
  //
  // TODO(crbug.com/952347): This check has to be done at DidStartNavigation
  // time, the HasUserGesture state is lost by the time the navigation
  // commits.
  if (!navigation_handle->IsRendererInitiated() ||
      navigation_handle->HasUserGesture()) {
    is_notification_prompt_cooldown_active_ = false;
  }
}

void PermissionRequestManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!queued_requests_.empty() || IsRequestInProgress()) {
    // |queued_requests_| and |requests_| will be deleted below, which
    // might be a problem for back-forward cache — the page might be restored
    // later, but the requests won't be.
    // Disable bfcache here if we have any requests here to prevent this
    // from happening.
    content::BackForwardCache::DisableForRenderFrameHost(
        navigation_handle->GetPreviousRenderFrameHostId(),
        back_forward_cache::DisabledReason(
            back_forward_cache::DisabledReasonId::kPermissionRequestManager));
  }

  CleanUpRequests();
}

void PermissionRequestManager::DocumentOnLoadCompletedInMainFrame(
    content::RenderFrameHost* render_frame_host) {
  // This is scheduled because while all calls to the browser have been
  // issued at DOMContentLoaded, they may be bouncing around in scheduled
  // callbacks finding the UI thread still. This makes sure we allow those
  // scheduled calls to AddRequest to complete before we show the page-load
  // permissions bubble.
  ScheduleDequeueRequestIfNeeded();
}

void PermissionRequestManager::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  ScheduleDequeueRequestIfNeeded();
}

void PermissionRequestManager::WebContentsDestroyed() {
  // If the web contents has been destroyed, treat the bubble as cancelled.
  CleanUpRequests();

  // The WebContents is going away; be aggressively paranoid and delete
  // ourselves lest other parts of the system attempt to add permission
  // bubbles or use us otherwise during the destruction.
  web_contents()->RemoveUserData(UserDataKey());
  // That was the equivalent of "delete this". This object is now destroyed;
  // returning from this function is the only safe thing to do.
}

void PermissionRequestManager::OnVisibilityChanged(
    content::Visibility visibility) {
  bool tab_was_hidden = tab_is_hidden_;
  tab_is_hidden_ = visibility == content::Visibility::HIDDEN;
  if (tab_was_hidden == tab_is_hidden_)
    return;

  if (tab_is_hidden_) {
    if (view_) {
      switch (view_->GetTabSwitchingBehavior()) {
        case PermissionPrompt::TabSwitchingBehavior::
            kDestroyPromptButKeepRequestPending:
          DeleteBubble();
          break;
        case PermissionPrompt::TabSwitchingBehavior::
            kDestroyPromptAndIgnoreRequest:
          FinalizeCurrentRequests(PermissionAction::IGNORED);
          break;
        case PermissionPrompt::TabSwitchingBehavior::kKeepPromptAlive:
          break;
      }
    }

    return;
  }

  if (!web_contents()->IsDocumentOnLoadCompletedInMainFrame())
    return;

  if (!IsRequestInProgress()) {
    ScheduleDequeueRequestIfNeeded();
    return;
  }

  if (view_) {
    // We switched tabs away and back while a prompt was active.
    DCHECK_EQ(view_->GetTabSwitchingBehavior(),
              PermissionPrompt::TabSwitchingBehavior::kKeepPromptAlive);
  } else if (current_request_ui_to_use_.has_value()) {
    ShowBubble();
  }
}

const std::vector<PermissionRequest*>& PermissionRequestManager::Requests() {
  return requests_;
}

GURL PermissionRequestManager::GetRequestingOrigin() const {
  CHECK(!requests_.empty());
  GURL origin = requests_.front()->requesting_origin();
  if (DCHECK_IS_ON()) {
    for (auto* request : requests_)
      DCHECK_EQ(origin, request->requesting_origin());
  }
  return origin;
}

GURL PermissionRequestManager::GetEmbeddingOrigin() const {
  return PermissionUtil::GetLastCommittedOriginAsURL(web_contents());
}

void PermissionRequestManager::Accept() {
  if (ignore_callbacks_from_prompt_)
    return;
  DCHECK(view_);
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    PermissionGrantedIncludingDuplicates(*requests_iter,
                                         /*is_one_time=*/false);
  }
  FinalizeCurrentRequests(PermissionAction::GRANTED);
}

void PermissionRequestManager::AcceptThisTime() {
  if (ignore_callbacks_from_prompt_)
    return;
  DCHECK(view_);
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    PermissionGrantedIncludingDuplicates(*requests_iter,
                                         /*is_one_time=*/true);
  }
  FinalizeCurrentRequests(PermissionAction::GRANTED_ONCE);
}

void PermissionRequestManager::Deny() {
  if (ignore_callbacks_from_prompt_)
    return;
  DCHECK(view_);

  // Suppress any further prompts in this WebContents, from any origin, until
  // there is a user-initiated navigation. This stops users from getting
  // trapped in request loops where the website automatically navigates
  // cross-origin (e.g. to another subdomain) to be able to prompt again after
  // a rejection.
  if (base::FeatureList::IsEnabled(
          features::kBlockRepeatedNotificationPermissionPrompts) &&
      std::any_of(requests_.begin(), requests_.end(), [](const auto* request) {
        return request->GetContentSettingsType() ==
               ContentSettingsType::NOTIFICATIONS;
      })) {
    is_notification_prompt_cooldown_active_ = true;
  }

  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    PermissionDeniedIncludingDuplicates(*requests_iter);
  }
  FinalizeCurrentRequests(PermissionAction::DENIED);
}

void PermissionRequestManager::Closing() {
  if (ignore_callbacks_from_prompt_)
    return;
  DCHECK(view_);
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    CancelledIncludingDuplicates(*requests_iter);
  }
  FinalizeCurrentRequests(PermissionAction::DISMISSED);
}

bool PermissionRequestManager::WasCurrentRequestAlreadyDisplayed() {
  return current_request_already_displayed_;
}

PermissionRequestManager::PermissionRequestManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      view_factory_(base::BindRepeating(&PermissionPrompt::Create)),
      view_(nullptr),
      tab_is_hidden_(web_contents->GetVisibility() ==
                     content::Visibility::HIDDEN),
      auto_response_for_test_(NONE),
      permission_ui_selectors_(
          PermissionsClient::Get()->CreatePermissionUiSelectors(
              web_contents->GetBrowserContext())) {}

void PermissionRequestManager::ScheduleShowBubble() {
  base::RecordAction(base::UserMetricsAction("PermissionBubbleRequest"));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PermissionRequestManager::ShowBubble,
                                weak_factory_.GetWeakPtr()));
}

void PermissionRequestManager::DequeueRequestIfNeeded() {
  // TODO(olesiamarukhno): Media requests block other media requests from
  // pre-empting them. For example, when a camera request is pending and mic
  // is requested, the camera request remains pending and mic request appears
  // only after the camera request is resolved. This is caused by code in
  // PermissionBubbleMediaAccessHandler and UserMediaClient. We probably don't
  // need two permission queues, so resolve the duplication.

  if (!web_contents()->IsDocumentOnLoadCompletedInMainFrame() || view_ ||
      IsRequestInProgress()) {
    return;
  }

  // Find first valid request.
  while (!queued_requests_.empty()) {
    PermissionRequest* next = PopNextQueuedRequest();
    PermissionRequestSource& source = request_sources_map_.find(next)->second;
    if (!source.IsSourceFrameInactiveAndDisallowActivation()) {
      requests_.push_back(next);
      break;
    }
    next->Cancelled();
    next->RequestFinished();
    request_sources_map_.erase(request_sources_map_.find(next));
  }

  if (requests_.empty()) {
    return;
  }

  // Find additional requests that can be grouped with the first one.
  for (; !queued_requests_.empty(); PopNextQueuedRequest()) {
    PermissionRequest* front = PeekNextQueuedRequest();
    PermissionRequestSource& source = request_sources_map_.find(front)->second;
    if (source.IsSourceFrameInactiveAndDisallowActivation()) {
      front->Cancelled();
      front->RequestFinished();
      request_sources_map_.erase(request_sources_map_.find(front));
    } else if (ShouldGroupRequests(requests_.front(), front)) {
      requests_.push_back(front);
    } else {
      break;
    }
  }

  if (!permission_ui_selectors_.empty()) {
    DCHECK(!current_request_ui_to_use_.has_value());
    // Initialize the selector decisions vector.
    DCHECK(selector_decisions_.empty());
    selector_decisions_.resize(permission_ui_selectors_.size());

    for (size_t selector_index = 0;
         selector_index < permission_ui_selectors_.size(); ++selector_index) {
      if (permission_ui_selectors_[selector_index]
              ->IsPermissionRequestSupported(
                  requests_.front()->request_type())) {
        permission_ui_selectors_[selector_index]->SelectUiToUse(
            requests_.front(),
            base::BindOnce(
                &PermissionRequestManager::OnPermissionUiSelectorDone,
                weak_factory_.GetWeakPtr(), selector_index));
      } else {
        OnPermissionUiSelectorDone(
            selector_index,
            PermissionUiSelector::Decision::UseNormalUiAndShowNoWarning());
      }
    }
  } else {
    current_request_ui_to_use_ =
        UiDecision(UiDecision::UseNormalUi(), UiDecision::ShowNoWarning());
    ScheduleShowBubble();
  }
}

void PermissionRequestManager::ScheduleDequeueRequestIfNeeded() {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PermissionRequestManager::DequeueRequestIfNeeded,
                     weak_factory_.GetWeakPtr()));
}

void PermissionRequestManager::ShowBubble() {
  // There is a race condition where the request might have been removed
  // already so double-checking that there is a request in progress.
  //
  // There is no need to show a new bubble if the previous one still exists.
  if (!IsRequestInProgress() || view_)
    return;

  DCHECK(web_contents()->IsDocumentOnLoadCompletedInMainFrame());
  DCHECK(current_request_ui_to_use_);

  if (tab_is_hidden_)
    return;

  view_ = view_factory_.Run(web_contents(), this);
  if (!view_) {
    current_request_prompt_disposition_ =
        PermissionPromptDisposition::NONE_VISIBLE;
    FinalizeCurrentRequests(PermissionAction::IGNORED);
    return;
  }

  current_request_prompt_disposition_ = view_->GetPromptDisposition();

  if (!current_request_already_displayed_) {
    PermissionUmaUtil::PermissionPromptShown(requests_);

    auto quiet_ui_reason = ReasonForUsingQuietUi();
    if (quiet_ui_reason) {
      switch (*quiet_ui_reason) {
        case QuietUiReason::kEnabledInPrefs:
        case QuietUiReason::kTriggeredByCrowdDeny:
        case QuietUiReason::kPredictedVeryUnlikelyGrant:
          break;
        case QuietUiReason::kTriggeredDueToAbusiveRequests:
          LogWarningToConsole(kAbusiveNotificationRequestsEnforcementMessage);
          break;
        case QuietUiReason::kTriggeredDueToAbusiveContent:
          LogWarningToConsole(kAbusiveNotificationContentEnforcementMessage);
          break;
      }
      base::RecordAction(base::UserMetricsAction(
          "Notifications.Quiet.PermissionRequestShown"));
    }
  }
  current_request_already_displayed_ = true;
  current_request_first_display_time_ = base::Time::Now();
  NotifyBubbleAdded();
  // If in testing mode, automatically respond to the bubble that was shown.
  if (auto_response_for_test_ != NONE)
    DoAutoResponseForTesting();
}

void PermissionRequestManager::DeleteBubble() {
  DCHECK(view_);
  {
    base::AutoReset<bool> deleting(&ignore_callbacks_from_prompt_, true);
    view_.reset();
  }
  NotifyBubbleRemoved();
}

void PermissionRequestManager::ResetViewStateForCurrentRequest() {
  for (const auto& selector : permission_ui_selectors_)
    selector->Cancel();

  current_request_already_displayed_ = false;
  current_request_first_display_time_ = base::Time();
  current_request_prompt_disposition_.reset();
  prediction_grant_likelihood_.reset();
  current_request_ui_to_use_.reset();
  selector_decisions_.clear();

  if (view_)
    DeleteBubble();
}

void PermissionRequestManager::FinalizeCurrentRequests(
    PermissionAction permission_action) {
  DCHECK(IsRequestInProgress());
  base::TimeDelta time_to_decision;
  if (!current_request_first_display_time_.is_null() &&
      permission_action != PermissionAction::IGNORED) {
    time_to_decision = base::Time::Now() - current_request_first_display_time_;
  }
  PermissionUmaUtil::PermissionPromptResolved(
      requests_, web_contents(), permission_action, time_to_decision,
      DetermineCurrentRequestUIDispositionForUMA(),
      DetermineCurrentRequestUIDispositionReasonForUMA(),
      prediction_grant_likelihood_);

  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();
  PermissionDecisionAutoBlocker* autoblocker =
      PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          browser_context);

  absl::optional<QuietUiReason> quiet_ui_reason;
  if (ShouldCurrentRequestUseQuietUI())
    quiet_ui_reason = ReasonForUsingQuietUi();

  for (PermissionRequest* request : requests_) {
    // TODO(timloh): We only support dismiss and ignore embargo for
    // permissions which use PermissionRequestImpl as the other subclasses
    // don't support GetContentSettingsType.
    if (request->GetContentSettingsType() == ContentSettingsType::DEFAULT)
      continue;

    PermissionsClient::Get()->OnPromptResolved(
        browser_context, request->request_type(), permission_action,
        request->requesting_origin(), quiet_ui_reason);

    PermissionEmbargoStatus embargo_status =
        PermissionEmbargoStatus::NOT_EMBARGOED;
    if (permission_action == PermissionAction::DISMISSED) {
      if (autoblocker->RecordDismissAndEmbargo(
              request->requesting_origin(), request->GetContentSettingsType(),
              ShouldCurrentRequestUseQuietUI())) {
        embargo_status = PermissionEmbargoStatus::REPEATED_DISMISSALS;
      }
    } else if (permission_action == PermissionAction::IGNORED) {
      if (autoblocker->RecordIgnoreAndEmbargo(
              request->requesting_origin(), request->GetContentSettingsType(),
              ShouldCurrentRequestUseQuietUI())) {
        embargo_status = PermissionEmbargoStatus::REPEATED_IGNORES;
      }
    }
    PermissionUmaUtil::RecordEmbargoStatus(embargo_status);
  }
  ResetViewStateForCurrentRequest();
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    RequestFinishedIncludingDuplicates(*requests_iter);
    request_sources_map_.erase(request_sources_map_.find(*requests_iter));
  }
  requests_.clear();

  for (Observer& observer : observer_list_)
    observer.OnRequestsFinalized();

  ScheduleDequeueRequestIfNeeded();
}

void PermissionRequestManager::CleanUpRequests() {
  for (auto* queued_request : queued_requests_) {
    CancelledIncludingDuplicates(queued_request);
    RequestFinishedIncludingDuplicates(queued_request);
    request_sources_map_.erase(request_sources_map_.find(queued_request));
  }
  queued_requests_.clear();

  if (IsRequestInProgress()) {
    std::vector<PermissionRequest*>::iterator requests_iter;
    for (requests_iter = requests_.begin(); requests_iter != requests_.end();
         requests_iter++) {
      CancelledIncludingDuplicates(*requests_iter);
    }
    FinalizeCurrentRequests(PermissionAction::IGNORED);
  }
}

PermissionRequest* PermissionRequestManager::GetExistingRequest(
    PermissionRequest* request) {
  for (PermissionRequest* existing_request : requests_) {
    if (request->IsDuplicateOf(existing_request))
      return existing_request;
  }
  for (PermissionRequest* queued_request : queued_requests_) {
    if (request->IsDuplicateOf(queued_request))
      return queued_request;
  }
  return nullptr;
}

void PermissionRequestManager::PermissionGrantedIncludingDuplicates(
    PermissionRequest* request,
    bool is_one_time) {
  DCHECK_EQ(1, base::STLCount(requests_, request) +
                   base::STLCount(queued_requests_, request))
      << "Only requests in [queued_[frame_]]requests_ can have duplicates";
  request->PermissionGranted(is_one_time);
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->PermissionGranted(is_one_time);
}

void PermissionRequestManager::PermissionDeniedIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(1, base::STLCount(requests_, request) +
                   base::STLCount(queued_requests_, request))
      << "Only requests in [queued_]requests_ can have duplicates";
  request->PermissionDenied();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->PermissionDenied();
}

void PermissionRequestManager::CancelledIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(1, base::STLCount(requests_, request) +
                   base::STLCount(queued_requests_, request))
      << "Only requests in [queued_]requests_ can have duplicates";
  request->Cancelled();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->Cancelled();
}

void PermissionRequestManager::RequestFinishedIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(1, base::STLCount(requests_, request) +
                   base::STLCount(queued_requests_, request))
      << "Only requests in [queued_]requests_ can have duplicates";
  request->RequestFinished();
  // Beyond this point, |request| has probably been deleted.
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->RequestFinished();
  // Additionally, we can now remove the duplicates.
  duplicate_requests_.erase(request);
}

void PermissionRequestManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PermissionRequestManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool PermissionRequestManager::ShouldCurrentRequestUseQuietUI() const {
  // ContentSettingImageModel might call into this method if the user switches
  // between tabs while the |notification_permission_ui_selectors_| are
  // pending.
  return ReasonForUsingQuietUi() != absl::nullopt;
}

absl::optional<PermissionRequestManager::QuietUiReason>
PermissionRequestManager::ReasonForUsingQuietUi() const {
  if (!IsRequestInProgress() || !current_request_ui_to_use_ ||
      !current_request_ui_to_use_->quiet_ui_reason)
    return absl::nullopt;

  return *(current_request_ui_to_use_->quiet_ui_reason);
}

bool PermissionRequestManager::IsRequestInProgress() const {
  return !requests_.empty();
}

bool PermissionRequestManager::ShouldDropCurrentRequestIfCannotShowQuietly() {
  absl::optional<QuietUiReason> quiet_ui_reason = ReasonForUsingQuietUi();
  if (quiet_ui_reason.has_value()) {
    switch (quiet_ui_reason.value()) {
      case QuietUiReason::kEnabledInPrefs:
      case QuietUiReason::kPredictedVeryUnlikelyGrant:
      case QuietUiReason::kTriggeredByCrowdDeny:
        return false;
      case QuietUiReason::kTriggeredDueToAbusiveRequests:
      case QuietUiReason::kTriggeredDueToAbusiveContent:
        return true;
    }
  }

  return false;
}

void PermissionRequestManager::NotifyBubbleAdded() {
  for (Observer& observer : observer_list_)
    observer.OnBubbleAdded();
}

void PermissionRequestManager::NotifyBubbleRemoved() {
  for (Observer& observer : observer_list_)
    observer.OnBubbleRemoved();
}

void PermissionRequestManager::OnPermissionUiSelectorDone(
    size_t selector_index,
    const UiDecision& decision) {
  if (decision.warning_reason) {
    switch (*(decision.warning_reason)) {
      case WarningReason::kAbusiveRequests:
        LogWarningToConsole(kAbusiveNotificationRequestsWarningMessage);
        break;
      case WarningReason::kAbusiveContent:
        LogWarningToConsole(kAbusiveNotificationContentWarningMessage);
        break;
    }
  }

  // We have already made a decision because of a higher priority selector
  // therefore this selector's decision can be discarded.
  if (current_request_ui_to_use_.has_value())
    return;

  CHECK_LT(selector_index, selector_decisions_.size());
  selector_decisions_[selector_index] = decision;

  size_t decision_index = 0;
  while (decision_index < selector_decisions_.size() &&
         selector_decisions_[decision_index].has_value()) {
    const UiDecision& current_decision =
        selector_decisions_[decision_index].value();

    if (!prediction_grant_likelihood_.has_value()) {
      prediction_grant_likelihood_ = permission_ui_selectors_[decision_index]
                                         ->PredictedGrantLikelihoodForUKM();
    }

    if (current_decision.quiet_ui_reason.has_value()) {
      current_request_ui_to_use_ = current_decision;
      break;
    }

    ++decision_index;
  }

  // All decisions have been considered and none was conclusive.
  if (decision_index == selector_decisions_.size() &&
      !current_request_ui_to_use_.has_value()) {
    current_request_ui_to_use_ = UiDecision::UseNormalUiAndShowNoWarning();
  }

  if (current_request_ui_to_use_.has_value()) {
    ScheduleShowBubble();
  }
}

PermissionPromptDisposition
PermissionRequestManager::DetermineCurrentRequestUIDispositionForUMA() {
  if (current_request_prompt_disposition_.has_value())
    return current_request_prompt_disposition_.value();
  return PermissionPromptDisposition::NONE_VISIBLE;
}

PermissionPromptDispositionReason
PermissionRequestManager::DetermineCurrentRequestUIDispositionReasonForUMA() {
  auto quiet_ui_reason = ReasonForUsingQuietUi();
  if (!quiet_ui_reason)
    return PermissionPromptDispositionReason::DEFAULT_FALLBACK;
  switch (*quiet_ui_reason) {
    case QuietUiReason::kEnabledInPrefs:
      return PermissionPromptDispositionReason::USER_PREFERENCE_IN_SETTINGS;
    case QuietUiReason::kTriggeredByCrowdDeny:
    case QuietUiReason::kTriggeredDueToAbusiveRequests:
    case QuietUiReason::kTriggeredDueToAbusiveContent:
      return PermissionPromptDispositionReason::SAFE_BROWSING_VERDICT;
    case QuietUiReason::kPredictedVeryUnlikelyGrant:
      return PermissionPromptDispositionReason::PREDICTION_SERVICE;
  }
}

void PermissionRequestManager::LogWarningToConsole(const char* message) {
  web_contents()->GetMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning, message);
}

void PermissionRequestManager::DoAutoResponseForTesting() {
  switch (auto_response_for_test_) {
    case ACCEPT_ONCE:
      AcceptThisTime();
      break;
    case ACCEPT_ALL:
      Accept();
      break;
    case DENY_ALL:
      Deny();
      break;
    case DISMISS:
      Closing();
      break;
    case NONE:
      NOTREACHED();
  }
}

PermissionRequest* PermissionRequestManager::PeekNextQueuedRequest() {
  return base::FeatureList::IsEnabled(features::kPermissionChip)
             ? queued_requests_.back()
             : queued_requests_.front();
}

PermissionRequest* PermissionRequestManager::PopNextQueuedRequest() {
  PermissionRequest* next = PeekNextQueuedRequest();
  if (base::FeatureList::IsEnabled(features::kPermissionChip))
    queued_requests_.pop_back();
  else
    queued_requests_.pop_front();
  return next;
}

void PermissionRequestManager::PushQueuedRequest(PermissionRequest* request) {
  if (base::FeatureList::IsEnabled(features::kPermissionQuietChip) &&
      !base::FeatureList::IsEnabled(features::kPermissionChip)) {
    queued_requests_.push_front(request);
  } else {
    queued_requests_.push_back(request);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PermissionRequestManager)

}  // namespace permissions
