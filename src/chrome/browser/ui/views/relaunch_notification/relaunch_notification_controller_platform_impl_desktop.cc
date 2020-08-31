// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_controller_platform_impl_desktop.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_metrics.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_recommended_bubble_view.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_dialog_view.h"
#include "chrome/common/buildflags.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

namespace {

// Returns the reason why a dialog was not shown when the conditions were ripe
// for such.
relaunch_notification::ShowResult GetNotShownReason() {
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  BackgroundModeManager* background_mode_manager =
      g_browser_process->background_mode_manager();
  if (background_mode_manager &&
      background_mode_manager->IsBackgroundWithoutWindows()) {
    return relaunch_notification::ShowResult::kBackgroundModeNoWindows;
  }
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
  return relaunch_notification::ShowResult::kUnknownNotShownReason;
}

// Returns the last active tabbed browser.
Browser* FindLastActiveTabbedBrowser() {
  BrowserList* browser_list = BrowserList::GetInstance();
  const auto end = browser_list->end_last_active();
  for (auto scan = browser_list->begin_last_active(); scan != end; ++scan) {
    if ((*scan)->is_type_normal())
      return *scan;
  }
  return nullptr;
}

}  // namespace

RelaunchNotificationControllerPlatformImpl::
    RelaunchNotificationControllerPlatformImpl() = default;

RelaunchNotificationControllerPlatformImpl::
    ~RelaunchNotificationControllerPlatformImpl() {
  DCHECK(!widget_);
  if (on_visible_)
    BrowserList::RemoveObserver(this);
}

void RelaunchNotificationControllerPlatformImpl::NotifyRelaunchRecommended(
    base::Time detection_time,
    bool /*past_deadline*/) {
  // Nothing to do if the bubble is visible.
  if (widget_)
    return;

  // Show the bubble in the most recently active browser.
  Browser* browser = FindLastActiveTabbedBrowser();
  relaunch_notification::RecordRecommendedShowResult(
      browser ? relaunch_notification::ShowResult::kShown
              : GetNotShownReason());
  if (!browser)
    return;

  widget_ = RelaunchRecommendedBubbleView::ShowBubble(
      browser, detection_time, base::BindRepeating(&chrome::AttemptRelaunch));

  // Monitor the widget so that |widget_| can be cleared on close.
  widget_->AddObserver(this);
}

void RelaunchNotificationControllerPlatformImpl::NotifyRelaunchRequired(
    base::Time deadline,
    base::OnceCallback<base::Time()> on_visible) {
  // Nothing to do if the dialog is visible.
  if (widget_)
    return;

  // Show the dialog in the active tabbed browser window.
  Browser* browser = chrome::FindBrowserWithActiveWindow();
  if (browser && browser->is_type_normal()) {
    DCHECK(!on_visible_);
    ShowRequiredNotification(browser, deadline);
    relaunch_notification::RecordRequiredShowResult(
        relaunch_notification::ShowResult::kShown);
    return;
  }

  relaunch_notification::RecordRequiredShowResult(GetNotShownReason());

  // If the instance is not already waiting for one to become active from a
  // previous call, start observing now.
  if (!on_visible_)
    BrowserList::AddObserver(this);

  // Hold on to the callback until an active tabbed browser is found.
  on_visible_ = std::move(on_visible);

  last_relaunch_deadline_ = deadline;
}

void RelaunchNotificationControllerPlatformImpl::CloseRelaunchNotification() {
  if (widget_)
    widget_->Close();
  if (on_visible_) {
    BrowserList::RemoveObserver(this);
    on_visible_.Reset();
    last_relaunch_deadline_ = base::Time();
  }
  has_shown_ = false;
}

void RelaunchNotificationControllerPlatformImpl::SetDeadline(
    base::Time deadline) {
  // Nothing to do if the dialog hasn't been shown yet (because no tabbed
  // browser has become active) or if the user has seen and dismissed the
  // dialog.
  if (widget_)
    RelaunchRequiredDialogView::FromWidget(widget_)->SetDeadline(deadline);

  // Hold on to the new deadline if the instance is waiting for a Browser to
  // become active.
  if (on_visible_)
    last_relaunch_deadline_ = deadline;
}

bool RelaunchNotificationControllerPlatformImpl::IsRequiredNotificationShown()
    const {
  return widget_ != nullptr;
}

void RelaunchNotificationControllerPlatformImpl::OnWidgetClosing(
    views::Widget* widget) {
  DCHECK_EQ(widget, widget_);
  widget->RemoveObserver(this);
  widget_ = nullptr;
}

void RelaunchNotificationControllerPlatformImpl::OnWidgetDestroying(
    views::Widget* widget) {
  DCHECK_EQ(widget, widget_);
  widget->RemoveObserver(this);
  widget_ = nullptr;
}

void RelaunchNotificationControllerPlatformImpl::OnBrowserSetLastActive(
    Browser* browser) {
  // Ignore non-tabbed browsers.
  if (!browser->is_type_normal())
    return;

  BrowserList::RemoveObserver(this);

  base::Time new_deadline =
      has_shown_ ? last_relaunch_deadline_ : std::move(on_visible_).Run();
  DCHECK(!new_deadline.is_null());

  on_visible_.Reset();
  last_relaunch_deadline_ = base::Time();

  ShowRequiredNotification(browser, new_deadline);
}

void RelaunchNotificationControllerPlatformImpl::ShowRequiredNotification(
    Browser* browser,
    base::Time deadline) {
  widget_ = RelaunchRequiredDialogView::Show(
      browser, deadline, base::BindRepeating(&chrome::AttemptRelaunch));
  has_shown_ = true;

  // Monitor the widget so that |widget_| can be cleared on close/destruction.
  widget_->AddObserver(this);
}
