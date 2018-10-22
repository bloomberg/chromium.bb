// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_ui_model.h"

#include "ash/assistant/model/assistant_ui_model_observer.h"

namespace ash {

AssistantUiModel::AssistantUiModel() = default;

AssistantUiModel::~AssistantUiModel() = default;

void AssistantUiModel::AddObserver(AssistantUiModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantUiModel::RemoveObserver(AssistantUiModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantUiModel::SetUiMode(AssistantUiMode ui_mode) {
  if (ui_mode == ui_mode_)
    return;

  ui_mode_ = ui_mode;
  NotifyUiModeChanged();
}

void AssistantUiModel::SetVisibility(AssistantVisibility visibility,
                                     AssistantSource source) {
  if (visibility == visibility_)
    return;

  const AssistantVisibility old_visibility = visibility_;
  visibility_ = visibility;

  NotifyUiVisibilityChanged(old_visibility, source);
}

void AssistantUiModel::NotifyUiModeChanged() {
  for (AssistantUiModelObserver& observer : observers_)
    observer.OnUiModeChanged(ui_mode_);
}

void AssistantUiModel::NotifyUiVisibilityChanged(
    AssistantVisibility old_visibility,
    AssistantSource source) {
  for (AssistantUiModelObserver& observer : observers_)
    observer.OnUiVisibilityChanged(visibility_, old_visibility, source);
}

}  // namespace ash
