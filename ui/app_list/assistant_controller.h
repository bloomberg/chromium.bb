// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_ASSISTANT_CONTROLLER_H_
#define UI_APP_LIST_ASSISTANT_CONTROLLER_H_

#include <string>

#include "base/macros.h"

namespace app_list {

class AssistantInteractionModelObserver;

class AssistantController {
 public:
  // Adds/removes the specified interaction |observer|.
  virtual void AddInteractionModelObserver(
      AssistantInteractionModelObserver* observer) = 0;
  virtual void RemoveInteractionModelObserver(
      AssistantInteractionModelObserver* observer) = 0;

  // Invoked on suggestion chip pressed event.
  virtual void OnSuggestionChipPressed(const std::string& text) = 0;

 protected:
  AssistantController() = default;
  virtual ~AssistantController() = default;

  DISALLOW_COPY_AND_ASSIGN(AssistantController);
};

}  // namespace app_list

#endif  // UI_APP_LIST_ASSISTANT_CONTROLLER_H_
