// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge_controller.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/clipboard_nudge.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/util/values/values_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {
namespace {

// Keys for tooltip sub-preferences for shown count and last time shown.
constexpr char kShownCount[] = "shown_count";
constexpr char kLastTimeShown[] = "last_time_shown";
constexpr char kNewFeatureBadgeCount[] = "new_feature_shown_count";

// The maximum number of 1 second buckets used to record the time between
// showing the nudge and recording the feature being opened/used.
constexpr int kBucketCount = 61;

}  // namespace

// A class for observing the clipboard nudge fade out animation. Once the fade
// out animation is complete the clipboard nudge will be destroyed.
class ImplicitNudgeHideAnimationObserver
    : public ui::ImplicitAnimationObserver {
 public:
  ImplicitNudgeHideAnimationObserver(std::unique_ptr<ClipboardNudge> nudge,
                                     ClipboardNudgeController* controller)
      : nudge_(std::move(nudge)), controller_(controller) {
    DCHECK(nudge_);
    DCHECK(controller_);
  }
  ImplicitNudgeHideAnimationObserver(
      const ImplicitNudgeHideAnimationObserver&) = delete;
  ImplicitNudgeHideAnimationObserver& operator=(
      const ImplicitNudgeHideAnimationObserver&) = delete;
  ~ImplicitNudgeHideAnimationObserver() override {
    StopObservingImplicitAnimations();
    nudge_->Close();
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    // |this| is deleted by the controller which owns  the observer.
    controller_->ForceCloseAnimatingNudge();
  }

 private:
  std::unique_ptr<ClipboardNudge> nudge_;
  // Owned by the shell.
  ClipboardNudgeController* const controller_;
};

ClipboardNudgeController::ClipboardNudgeController(
    ClipboardHistory* clipboard_history,
    ClipboardHistoryControllerImpl* clipboard_history_controller)
    : clipboard_history_(clipboard_history),
      clipboard_history_controller_(clipboard_history_controller) {
  clipboard_history_->AddObserver(this);
  clipboard_history_controller_->AddObserver(this);
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);
  if (chromeos::features::IsClipboardHistoryNudgeSessionResetEnabled())
    Shell::Get()->session_controller()->AddObserver(this);
}

ClipboardNudgeController::~ClipboardNudgeController() {
  hide_nudge_animation_observer_.reset();
  clipboard_history_->RemoveObserver(this);
  clipboard_history_controller_->RemoveObserver(this);
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
  if (chromeos::features::IsClipboardHistoryNudgeSessionResetEnabled())
    Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void ClipboardNudgeController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kMultipasteNudges);
}

void ClipboardNudgeController::OnClipboardHistoryItemAdded(
    const ClipboardHistoryItem& item,
    bool is_duplicate) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!ShouldShowNudge(prefs))
    return;

  switch (clipboard_state_) {
    case ClipboardState::kInit:
      clipboard_state_ = ClipboardState::kFirstCopy;
      return;
    case ClipboardState::kFirstPaste:
      clipboard_state_ = ClipboardState::kSecondCopy;
      return;
    case ClipboardState::kFirstCopy:
    case ClipboardState::kSecondCopy:
    case ClipboardState::kShouldShowNudge:
      return;
  }
}

void ClipboardNudgeController::MarkNewFeatureBadgeShown() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs)
    return;
  const int shown_count = GetNewFeatureBadgeShownCount(prefs);
  DictionaryPrefUpdate update(prefs, prefs::kMultipasteNudges);
  update->SetIntPath(kNewFeatureBadgeCount, shown_count + 1);
}

bool ClipboardNudgeController::ShouldShowNewFeatureBadge() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs)
    return false;
  int badge_shown_count = GetNewFeatureBadgeShownCount(prefs);
  // We should not show more nudges after hitting the limit.
  return badge_shown_count < kContextMenuBadgeShowLimit;
}

void ClipboardNudgeController::OnClipboardDataRead() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!ClipboardHistoryUtil::IsEnabledInCurrentMode() || !prefs ||
      !ShouldShowNudge(prefs)) {
    return;
  }

  switch (clipboard_state_) {
    case ClipboardState::kFirstCopy:
      clipboard_state_ = ClipboardState::kFirstPaste;
      last_paste_timestamp_ = GetTime();
      return;
    case ClipboardState::kFirstPaste:
      // Subsequent pastes should reset the timestamp.
      last_paste_timestamp_ = GetTime();
      return;
    case ClipboardState::kSecondCopy:
      if (GetTime() - last_paste_timestamp_ < kMaxTimeBetweenPaste) {
        ShowNudge(ClipboardNudgeType::kOnboardingNudge);
        HandleNudgeShown();
      } else {
        // ClipboardState should be reset to kFirstPaste when timed out.
        clipboard_state_ = ClipboardState::kFirstPaste;
        last_paste_timestamp_ = GetTime();
      }
      return;
    case ClipboardState::kInit:
    case ClipboardState::kShouldShowNudge:
      return;
  }
}

void ClipboardNudgeController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  // Reset the nudge prefs so that the nudge can be shown again.
  DictionaryPrefUpdate update(prefs, prefs::kMultipasteNudges);
  update->SetIntPath(kShownCount, 0);
  update->SetPath(kLastTimeShown, util::TimeToValue(base::Time()));
  update->SetIntPath(kNewFeatureBadgeCount, 0);
}

void ClipboardNudgeController::ShowNudge(ClipboardNudgeType nudge_type) {
  if (nudge_ && !nudge_->widget()->IsClosed()) {
    hide_nudge_timer_.AbandonAndStop();
    nudge_->Close();
  }

  // Create and show the nudge.
  nudge_ = std::make_unique<ClipboardNudge>(nudge_type);
  StartFadeAnimation(/*show=*/true);

  // Start a timer to close the nudge after a set amount of time.
  hide_nudge_timer_.Start(FROM_HERE, kNudgeShowTime,
                          base::BindOnce(&ClipboardNudgeController::HideNudge,
                                         weak_ptr_factory_.GetWeakPtr()));

  // Tracks the number of times the ClipboardHistory nudge is shown.
  // This allows us to understand the conversion rate of showing a nudge to
  // a user opening and then using the clipboard history feature.
  switch (nudge_type) {
    case ClipboardNudgeType::kOnboardingNudge:
      last_shown_time_ = GetTime();
      base::UmaHistogramExactLinear(
          "Ash.ClipboardHistory.ContextualNudge.ShownCount", 1, 1);
      break;
    case ClipboardNudgeType::kZeroStateNudge:
      base::UmaHistogramExactLinear(
          "Ash.ClipboardHistory.ZeroStateContextualNudge.ShownCount", 1, 1);
      break;
    default:
      NOTREACHED();
  }
}

void ClipboardNudgeController::HideNudge() {
  StartFadeAnimation(/*show=*/false);
}

void ClipboardNudgeController::StartFadeAnimation(bool show) {
  // Clean any pending animation observer.
  hide_nudge_animation_observer_.reset();

  ui::Layer* layer = nudge_->widget()->GetLayer();
  gfx::Rect widget_bounds = layer->bounds();

  gfx::Transform scaled_nudge_transform;
  float x_offset =
      widget_bounds.width() * (1.0f - kNudgeFadeAnimationScale) / 2.0f;
  float y_offset =
      widget_bounds.height() * (1.0f - kNudgeFadeAnimationScale) / 2.0f;
  scaled_nudge_transform.Translate(x_offset, y_offset);
  scaled_nudge_transform.Scale(kNudgeFadeAnimationScale,
                               kNudgeFadeAnimationScale);

  layer->SetOpacity(show ? 0.0f : 1.0f);
  layer->SetTransform(show ? scaled_nudge_transform : gfx::Transform());

  {
    // Perform the scaling animation on the clipboard nudge.
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(kNudgeFadeAnimationTime);
    settings.SetTweenType(kNudgeFadeScalingAnimationTweenType);
    layer->SetTransform(show ? gfx::Transform() : scaled_nudge_transform);
  }
  {
    // Perform the opacity animation on the clipboard nudge.
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(kNudgeFadeAnimationTime);
    settings.SetTweenType(kNudgeFadeOpacityAnimationTweenType);
    layer->SetOpacity(show ? 1.0f : 0.0f);
    if (!show) {
      hide_nudge_animation_observer_ =
          std::make_unique<ImplicitNudgeHideAnimationObserver>(
              std::move(nudge_), this);
      settings.AddObserver(hide_nudge_animation_observer_.get());
    }
  }
}

void ClipboardNudgeController::HandleNudgeShown() {
  clipboard_state_ = ClipboardState::kInit;
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs)
    return;
  const int shown_count = GetShownCount(prefs);
  DictionaryPrefUpdate update(prefs, prefs::kMultipasteNudges);
  update->SetIntPath(kShownCount, shown_count + 1);
  update->SetPath(kLastTimeShown, util::TimeToValue(GetTime()));
}

void ClipboardNudgeController::OnClipboardHistoryMenuShown() {
  if (last_shown_time_.is_null())
    return;
  base::TimeDelta time_since_shown = GetTime() - last_shown_time_;

  // Tracks the amount of time between showing the user a nudge and the user
  // opening the ClipboardHistory menu.
  base::UmaHistogramExactLinear(
      "Ash.ClipboardHistory.ContextualNudge.NudgeToFeatureOpenTime",
      time_since_shown.InSeconds(), kBucketCount);
}

void ClipboardNudgeController::OnClipboardHistoryPasted() {
  if (last_shown_time_.is_null())
    return;
  base::TimeDelta time_since_shown = GetTime() - last_shown_time_;

  // Tracks the amount of time between showing the user a nudge and the user
  // using the ClipboardHistory feature.
  base::UmaHistogramExactLinear(
      "Ash.ClipboardHistory.ContextualNudge.NudgeToFeatureUseTime",
      time_since_shown.InSeconds(), kBucketCount);
}

void ClipboardNudgeController::ForceCloseAnimatingNudge() {
  hide_nudge_animation_observer_.reset();
}

void ClipboardNudgeController::OverrideClockForTesting(
    base::Clock* test_clock) {
  DCHECK(!g_clock_override);
  g_clock_override = test_clock;
}

void ClipboardNudgeController::ClearClockOverrideForTesting() {
  DCHECK(g_clock_override);
  g_clock_override = nullptr;
}

void ClipboardNudgeController::FireHideNudgeTimerForTesting() {
  hide_nudge_timer_.FireNow();
}

const ClipboardState& ClipboardNudgeController::GetClipboardStateForTesting() {
  return clipboard_state_;
}

int ClipboardNudgeController::GetShownCount(PrefService* prefs) {
  const base::DictionaryValue* dictionary =
      prefs->GetDictionary(prefs::kMultipasteNudges);
  if (!dictionary)
    return 0;
  return dictionary->FindIntPath(kShownCount).value_or(0);
}

int ClipboardNudgeController::GetNewFeatureBadgeShownCount(PrefService* prefs) {
  const base::DictionaryValue* dictionary =
      prefs->GetDictionary(prefs::kMultipasteNudges);
  if (!dictionary)
    return 0;
  return dictionary->FindIntPath(kNewFeatureBadgeCount).value_or(0);
}

base::Time ClipboardNudgeController::GetLastShownTime(PrefService* prefs) {
  const base::DictionaryValue* dictionary =
      prefs->GetDictionary(prefs::kMultipasteNudges);
  if (!dictionary)
    return base::Time();
  base::Optional<base::Time> last_shown_time =
      util::ValueToTime(dictionary->FindPath(kLastTimeShown));
  return last_shown_time.value_or(base::Time());
}

bool ClipboardNudgeController::ShouldShowNudge(PrefService* prefs) {
  if (!prefs)
    return false;
  int nudge_shown_count = GetShownCount(prefs);
  base::Time last_shown_time = GetLastShownTime(prefs);
  // We should not show more nudges after hitting the limit.
  if (nudge_shown_count >= kNotificationLimit)
    return false;
  // If the nudge has yet to be shown, we should return true.
  if (last_shown_time.is_null())
    return true;

  // We should show the nudge if enough time has passed since the nudge was last
  // shown.
  return base::Time::Now() - last_shown_time > kMinInterval;
}

base::Time ClipboardNudgeController::GetTime() {
  if (g_clock_override)
    return g_clock_override->Now();
  return base::Time::Now();
}

}  // namespace ash
