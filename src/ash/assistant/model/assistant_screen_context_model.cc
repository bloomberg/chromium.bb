// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_screen_context_model.h"

#include "ash/assistant/model/assistant_screen_context_model_observer.h"

namespace ash {

AssistantScreenContextModel::AssistantScreenContextModel() = default;

AssistantScreenContextModel::~AssistantScreenContextModel() = default;

void AssistantScreenContextModel::AddObserver(
    AssistantScreenContextModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantScreenContextModel::RemoveObserver(
    AssistantScreenContextModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantScreenContextModel::SetRequestState(
    ScreenContextRequestState request_state) {
  if (request_state == request_state_)
    return;

  request_state_ = request_state;
  NotifyRequestStateChanged();
}

void AssistantScreenContextModel::NotifyRequestStateChanged() {
  for (AssistantScreenContextModelObserver& observer : observers_)
    observer.OnScreenContextRequestStateChanged(request_state_);
}

}  // namespace ash
