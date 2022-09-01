// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_audio_detailed_view_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/audio/audio_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

UnifiedAudioDetailedViewController::UnifiedAudioDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

UnifiedAudioDetailedViewController::~UnifiedAudioDetailedViewController() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

views::View* UnifiedAudioDetailedViewController::CreateView() {
  DCHECK(!view_);
  view_ = new AudioDetailedView(detailed_view_delegate_.get());
  view_->Update();
  return view_;
}

std::u16string UnifiedAudioDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_AUDIO_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

void UnifiedAudioDetailedViewController::OnAudioNodesChanged() {
  if (view_)
    view_->Update();
}

void UnifiedAudioDetailedViewController::OnActiveOutputNodeChanged() {
  if (view_)
    view_->Update();
}

void UnifiedAudioDetailedViewController::OnActiveInputNodeChanged() {
  if (view_)
    view_->Update();
}

}  // namespace ash
