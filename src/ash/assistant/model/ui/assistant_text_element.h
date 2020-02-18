// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_UI_ASSISTANT_TEXT_ELEMENT_H_
#define ASH_ASSISTANT_MODEL_UI_ASSISTANT_TEXT_ELEMENT_H_

#include <string>

#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "base/component_export.h"
#include "base/macros.h"

namespace ash {

// An Assistant UI element that will be rendered as text.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantTextElement
    : public AssistantUiElement {
 public:
  explicit AssistantTextElement(const std::string& text);
  ~AssistantTextElement() override;

  const std::string& text() const { return text_; }

 private:
  const std::string text_;

  DISALLOW_COPY_AND_ASSIGN(AssistantTextElement);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_UI_ASSISTANT_TEXT_ELEMENT_H_
