// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_UI_ASSISTANT_UI_ELEMENT_H_
#define ASH_ASSISTANT_MODEL_UI_ASSISTANT_UI_ELEMENT_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"

namespace ash {

// AssistantUiElementType ------------------------------------------------------

// Defines possible types of Assistant UI elements.
enum class AssistantUiElementType {
  kCard,    // See AssistantCardElement.
  kText,    // See AssistantTextElement.
};

// AssistantUiElement ----------------------------------------------------------

// Base class for a UI element that will be rendered inside of Assistant UI.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantUiElement {
 public:
  virtual ~AssistantUiElement();

  AssistantUiElementType type() const { return type_; }

  // Invoke to being processing the UI element for rendering. The specified
  // |callback| will be run upon completion. Note that we don't include the
  // processing result in the callback, as in |AssistantResponse| we handle
  // success/failure cases the same because failures will be skipped in view
  // handling.
  using ProcessingCallback = base::OnceCallback<void()>;
  virtual void Process(ProcessingCallback callback);

 protected:
  explicit AssistantUiElement(AssistantUiElementType type);

 private:
  const AssistantUiElementType type_;

  DISALLOW_COPY_AND_ASSIGN(AssistantUiElement);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_UI_ASSISTANT_UI_ELEMENT_H_
