// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_OBSERVER_H_

#include "base/macros.h"

namespace ash {

enum class ScreenContextRequestState;

class AssistantScreenContextModelObserver {
 public:
  // Invoked when the screen context request state is changed.
  virtual void OnScreenContextRequestStateChanged(
      ScreenContextRequestState request_state) {}

 protected:
  AssistantScreenContextModelObserver() = default;
  virtual ~AssistantScreenContextModelObserver() = default;

  DISALLOW_COPY_AND_ASSIGN(AssistantScreenContextModelObserver);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_OBSERVER_H_
