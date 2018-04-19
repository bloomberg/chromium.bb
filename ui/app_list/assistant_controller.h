// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_ASSISTANT_CONTROLLER_H_
#define UI_APP_LIST_ASSISTANT_CONTROLLER_H_

#include <string>

#include "ash/public/interfaces/assistant_card_renderer.mojom.h"
#include "base/macros.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace app_list {

class AssistantInteractionModelObserver;

// Interface for the Assistant controller in ash.
class AssistantController {
 public:
  // Registers the specified |observer| with the interaction model observer
  // pool.
  virtual void AddInteractionModelObserver(
      AssistantInteractionModelObserver* observer) = 0;

  // Unregisters the specified |observer| from the interaction model observer
  // pool.
  virtual void RemoveInteractionModelObserver(
      AssistantInteractionModelObserver* observer) = 0;

  // Renders a card, uniquely identified by |id_token|, according to the
  // specified |params|. When the card is ready for embedding, the supplied
  // |callback| is run with a token for embedding.
  virtual void RenderCard(
      const base::UnguessableToken& id_token,
      ash::mojom::AssistantCardParamsPtr params,
      ash::mojom::AssistantCardRenderer::RenderCallback callback) = 0;

  // Releases resources for the card uniquely identified by |id_token|.
  virtual void ReleaseCard(const base::UnguessableToken& id_token) = 0;

  // Invoked on suggestion chip pressed event.
  virtual void OnSuggestionChipPressed(const std::string& text) = 0;

 protected:
  AssistantController() = default;
  virtual ~AssistantController() = default;

  DISALLOW_COPY_AND_ASSIGN(AssistantController);
};

}  // namespace app_list

#endif  // UI_APP_LIST_ASSISTANT_CONTROLLER_H_
