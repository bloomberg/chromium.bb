// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

namespace content {

// Auto-disable accessibility if this many seconds elapse with user input
// events but no accessibility API usage.
constexpr int kAutoDisableAccessibilityTimeSecs = 30;

// Minimum number of user input events with no accessibility API usage
// before auto-disabling accessibility.
constexpr int kAutoDisableAccessibilityEventCount = 3;

// Updating Active/Inactive time on every accessibility api calls would not be
// good for perf. Instead, delay the update task.
constexpr int kOnAccessibilityUsageUpdateDelaySecs = 1;

// IMPORTANT!
// These values are written to logs.  Do not renumber or delete
// existing items; add new entries to the end of the list.
enum ModeFlagHistogramValue {
  UMA_AX_MODE_NATIVE_APIS = 0,
  UMA_AX_MODE_WEB_CONTENTS = 1,
  UMA_AX_MODE_INLINE_TEXT_BOXES = 2,
  UMA_AX_MODE_SCREEN_READER = 3,
  UMA_AX_MODE_HTML = 4,

  // This must always be the last enum. It's okay for its value to
  // increase, but none of the other enum values may change.
  UMA_AX_MODE_MAX
};

// Record a histograms for an accessibility mode when it's enabled.
void RecordNewAccessibilityModeFlags(ModeFlagHistogramValue mode_flag) {
  UMA_HISTOGRAM_ENUMERATION("Accessibility.ModeFlag", mode_flag,
                            UMA_AX_MODE_MAX);
}

// Update the accessibility histogram 45 seconds after initialization.
static const int ACCESSIBILITY_HISTOGRAM_DELAY_SECS = 45;

// static
BrowserAccessibilityState* BrowserAccessibilityState::GetInstance() {
  return BrowserAccessibilityStateImpl::GetInstance();
}

// On Android, Mac, and Windows there are platform-specific subclasses.
#if !defined(OS_ANDROID) && !defined(OS_WIN) && !defined(OS_MAC)
// static
BrowserAccessibilityStateImpl* BrowserAccessibilityStateImpl::GetInstance() {
  static base::NoDestructor<BrowserAccessibilityStateImpl> instance;
  return &*instance;
}
#endif

BrowserAccessibilityStateImpl::BrowserAccessibilityStateImpl()
    : BrowserAccessibilityState(),
      histogram_delay_(base::Seconds(ACCESSIBILITY_HISTOGRAM_DELAY_SECS)) {
  force_renderer_accessibility_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility);

  ResetAccessibilityModeValue();

  // Hook ourselves up to observe ax mode changes.
  ui::AXPlatformNode::AddAXModeObserver(this);
}

void BrowserAccessibilityStateImpl::InitBackgroundTasks() {
  // Schedule calls to update histograms after a delay.
  //
  // The delay is necessary because assistive technology sometimes isn't
  // detected until after the user interacts in some way, so a reasonable delay
  // gives us better numbers.

  // Some things can be done on another thread safely.
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          &BrowserAccessibilityStateImpl::UpdateHistogramsOnOtherThread,
          base::Unretained(this)),
      histogram_delay_);

  // Other things must be done on the UI thread (e.g. to access PrefService).
  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BrowserAccessibilityStateImpl::UpdateHistogramsOnUIThread,
                     base::Unretained(this)),
      histogram_delay_);
}

BrowserAccessibilityStateImpl::~BrowserAccessibilityStateImpl() {
  // Remove ourselves from the AXMode global observer list.
  ui::AXPlatformNode::RemoveAXModeObserver(this);
}

void BrowserAccessibilityStateImpl::OnScreenReaderDetected() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    return;
  }
  EnableAccessibility();
}

void BrowserAccessibilityStateImpl::EnableAccessibility() {
  AddAccessibilityModeFlags(ui::kAXModeComplete);
}

void BrowserAccessibilityStateImpl::DisableAccessibility() {
  ResetAccessibilityMode();
}

bool BrowserAccessibilityStateImpl::IsRendererAccessibilityEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableRendererAccessibility);
}

void BrowserAccessibilityStateImpl::ResetAccessibilityModeValue() {
  accessibility_mode_ = ui::AXMode();
  if (force_renderer_accessibility_)
    AddAccessibilityModeFlags(ui::kAXModeComplete);
}

void BrowserAccessibilityStateImpl::ResetAccessibilityMode() {
  ResetAccessibilityModeValue();

  std::vector<WebContentsImpl*> web_contents_vector =
      WebContentsImpl::GetAllWebContents();
  for (size_t i = 0; i < web_contents_vector.size(); ++i)
    web_contents_vector[i]->SetAccessibilityMode(accessibility_mode_);
}

bool BrowserAccessibilityStateImpl::IsAccessibleBrowser() {
  return accessibility_mode_ == ui::kAXModeComplete;
}

void BrowserAccessibilityStateImpl::AddUIThreadHistogramCallback(
    base::OnceClosure callback) {
  ui_thread_histogram_callbacks_.push_back(std::move(callback));
}

void BrowserAccessibilityStateImpl::AddOtherThreadHistogramCallback(
    base::OnceClosure callback) {
  other_thread_histogram_callbacks_.push_back(std::move(callback));
}

void BrowserAccessibilityStateImpl::UpdateHistogramsForTesting() {
  UpdateHistogramsOnUIThread();
  UpdateHistogramsOnOtherThread();
}

void BrowserAccessibilityStateImpl::SetCaretBrowsingState(bool enabled) {
  caret_browsing_enabled_ = enabled;
}

bool BrowserAccessibilityStateImpl::IsCaretBrowsingEnabled() const {
  return caret_browsing_enabled_;
}

void BrowserAccessibilityStateImpl::UpdateHistogramsOnUIThread() {
  for (auto& callback : ui_thread_histogram_callbacks_)
    std::move(callback).Run();
  ui_thread_histogram_callbacks_.clear();

  UMA_HISTOGRAM_BOOLEAN("Accessibility.ManuallyEnabled",
                        force_renderer_accessibility_);
#if defined(OS_WIN)
  UMA_HISTOGRAM_ENUMERATION(
      "Accessibility.WinHighContrastTheme",
      ui::NativeTheme::GetInstanceForNativeUi()
          ->GetPlatformHighContrastColorScheme(),
      ui::NativeTheme::PlatformHighContrastColorScheme::kMaxValue);
#endif

  ui_thread_done_ = true;
  if (other_thread_done_ && background_thread_done_callback_)
    std::move(background_thread_done_callback_).Run();
}

void BrowserAccessibilityStateImpl::UpdateHistogramsOnOtherThread() {
  for (auto& callback : other_thread_histogram_callbacks_)
    std::move(callback).Run();
  other_thread_histogram_callbacks_.clear();

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserAccessibilityStateImpl::OnOtherThreadDone,
                     base::Unretained(this)));
}

void BrowserAccessibilityStateImpl::OnOtherThreadDone() {
  other_thread_done_ = true;
  if (ui_thread_done_ && background_thread_done_callback_)
    std::move(background_thread_done_callback_).Run();
}

void BrowserAccessibilityStateImpl::UpdateAccessibilityActivityTask() {
  base::TimeTicks now = ui::EventTimeForNow();
  accessibility_last_usage_time_ = now;
  if (accessibility_active_start_time_.is_null())
    accessibility_active_start_time_ = now;
  // If accessibility was enabled but inactive until now, log the amount
  // of time between now and the last API usage.
  if (!accessibility_inactive_start_time_.is_null()) {
    base::UmaHistogramLongTimes("Accessibility.InactiveTime",
                                now - accessibility_inactive_start_time_);
    accessibility_inactive_start_time_ = base::TimeTicks();
  }
  accessibility_update_task_pending_ = false;
}

void BrowserAccessibilityStateImpl::OnAXModeAdded(ui::AXMode mode) {
  AddAccessibilityModeFlags(mode);
}

ui::AXMode BrowserAccessibilityStateImpl::GetAccessibilityMode() {
  return accessibility_mode_;
}

void BrowserAccessibilityStateImpl::OnUserInputEvent() {
  // No need to do anything if accessibility is off, or if it was forced on.
  if (accessibility_mode_.is_mode_off() || force_renderer_accessibility_)
    return;

  // If we get at least kAutoDisableAccessibilityEventCount user input
  // events, more than kAutoDisableAccessibilityTimeSecs apart, with
  // no accessibility API usage in-between disable accessibility.
  // (See also OnAccessibilityApiUsage()).
  base::TimeTicks now = ui::EventTimeForNow();
  user_input_event_count_++;
  if (user_input_event_count_ == 1) {
    first_user_input_event_time_ = now;
    return;
  }

  if (user_input_event_count_ < kAutoDisableAccessibilityEventCount)
    return;

  if (now - first_user_input_event_time_ >
      base::Seconds(kAutoDisableAccessibilityTimeSecs)) {
    if (!accessibility_active_start_time_.is_null()) {
      base::UmaHistogramLongTimes(
          "Accessibility.ActiveTime",
          accessibility_last_usage_time_ - accessibility_active_start_time_);

      // This will help track the time accessibility spends enabled, but
      // inactive.
      if (!features::IsAutoDisableAccessibilityEnabled())
        accessibility_inactive_start_time_ = accessibility_last_usage_time_;

      accessibility_active_start_time_ = base::TimeTicks();
    }

    // Check if the feature to auto-disable accessibility is even enabled.
    if (features::IsAutoDisableAccessibilityEnabled()) {
      base::UmaHistogramCounts1000("Accessibility.AutoDisabled.EventCount",
                                   user_input_event_count_);
      DCHECK(!accessibility_enabled_time_.is_null());
      base::UmaHistogramLongTimes("Accessibility.AutoDisabled.EnabledTime",
                                  now - accessibility_enabled_time_);

      accessibility_disabled_time_ = now;
      DisableAccessibility();
    }
  }
}

void BrowserAccessibilityStateImpl::OnAccessibilityApiUsage() {
  // See OnUserInputEvent for how this is used to disable accessibility.
  user_input_event_count_ = 0;

  // See comment above kOnAccessibilityUsageUpdateDelaySecs for why we post a
  // delayed task.
  if (!accessibility_update_task_pending_) {
    accessibility_update_task_pending_ = true;
    GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &BrowserAccessibilityStateImpl::UpdateAccessibilityActivityTask,
            base::Unretained(this)),
        base::Seconds(kOnAccessibilityUsageUpdateDelaySecs));
  }
}

void BrowserAccessibilityStateImpl::UpdateUniqueUserHistograms() {}

#if defined(OS_ANDROID)
void BrowserAccessibilityStateImpl::SetImageLabelsModeForProfile(
    bool enabled,
    BrowserContext* profile) {}
#endif

void BrowserAccessibilityStateImpl::AddAccessibilityModeFlags(ui::AXMode mode) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    return;
  }

  // Adding an accessibility mode flag is generally the result of an
  // accessibility API call, so we should also reset the auto-disable
  // accessibility code. The only exception is in tests or when a user manually
  // toggles accessibility flags in chrome://accessibility.
  OnAccessibilityApiUsage();

  ui::AXMode previous_mode = accessibility_mode_;
  accessibility_mode_ |= mode;
  if (accessibility_mode_ == previous_mode)
    return;

  // Keep track of the total time accessibility is enabled, and the time
  // it was previously disabled.
  if (previous_mode.is_mode_off()) {
    base::TimeTicks now = ui::EventTimeForNow();
    accessibility_enabled_time_ = now;
    if (!accessibility_disabled_time_.is_null()) {
      base::UmaHistogramLongTimes("Accessibility.AutoDisabled.DisabledTime",
                                  now - accessibility_disabled_time_);
    }
  }

  // Proxy the AXMode to AXPlatformNode to enable accessibility.
  ui::AXPlatformNode::NotifyAddAXModeFlags(accessibility_mode_);

  // Retrieve only newly added modes for the purposes of logging.
  int new_mode_flags = mode.mode() & (~previous_mode.mode());
  if (new_mode_flags & ui::AXMode::kNativeAPIs)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_NATIVE_APIS);
  if (new_mode_flags & ui::AXMode::kWebContents)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_WEB_CONTENTS);
  if (new_mode_flags & ui::AXMode::kInlineTextBoxes)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_INLINE_TEXT_BOXES);
  if (new_mode_flags & ui::AXMode::kScreenReader)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_SCREEN_READER);
  if (new_mode_flags & ui::AXMode::kHTML)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_HTML);

  std::vector<WebContentsImpl*> web_contents_vector =
      WebContentsImpl::GetAllWebContents();
  for (size_t i = 0; i < web_contents_vector.size(); ++i)
    web_contents_vector[i]->AddAccessibilityMode(accessibility_mode_);

  // Add a crash key with the ax_mode, to enable searching for top crashes that
  // occur when accessibility is turned on. This adds it for the browser
  // process, and elsewhere the same key is added to renderer processes.
  static auto* ax_mode_crash_key = base::debug::AllocateCrashKeyString(
      "ax_mode", base::debug::CrashKeySize::Size64);
  if (ax_mode_crash_key) {
    base::debug::SetCrashKeyString(ax_mode_crash_key,
                                   accessibility_mode_.ToString());
  }
}

void BrowserAccessibilityStateImpl::RemoveAccessibilityModeFlags(
    ui::AXMode mode) {
  if (force_renderer_accessibility_ && mode == ui::kAXModeComplete)
    return;

  int raw_flags =
      accessibility_mode_.mode() ^ (mode.mode() & accessibility_mode_.mode());
  accessibility_mode_ = raw_flags;

  // Proxy the new AXMode to AXPlatformNode.
  ui::AXPlatformNode::SetAXMode(accessibility_mode_);

  std::vector<WebContentsImpl*> web_contents_vector =
      WebContentsImpl::GetAllWebContents();
  for (size_t i = 0; i < web_contents_vector.size(); ++i)
    web_contents_vector[i]->SetAccessibilityMode(accessibility_mode_);
}

base::CallbackListSubscription
BrowserAccessibilityStateImpl::RegisterFocusChangedCallback(
    FocusChangedCallback callback) {
  return focus_changed_callbacks_.Add(std::move(callback));
}

void BrowserAccessibilityStateImpl::CallInitBackgroundTasksForTesting(
    base::RepeatingClosure done_callback) {
  // Set the delay to 1 second, that ensures that we actually test having
  // a nonzero delay but the test still runs quickly.
  histogram_delay_ = base::Seconds(1);
  background_thread_done_callback_ = done_callback;
  InitBackgroundTasks();
}

void BrowserAccessibilityStateImpl::OnFocusChangedInPage(
    const FocusedNodeDetails& details) {
  focus_changed_callbacks_.Notify(details);
}

}  // namespace content
