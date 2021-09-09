// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_CONVERSATION_OBSERVER_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_CONVERSATION_OBSERVER_H_

#include "base/component_export.h"
#include "chromeos/services/libassistant/public/cpp/assistant_interaction_metadata.h"
#include "chromeos/services/libassistant/public/cpp/assistant_suggestion.h"
#include "chromeos/services/libassistant/public/mojom/conversation_observer.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace assistant {

// Default implementation of |mojom::ConversationObserver|, which allow child
// child classes to only implement handlers they are interested in.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) ConversationObserver
    : public chromeos::libassistant::mojom::ConversationObserver {
 public:
  // chromeos::libassistant::mojom::ConversationObserver:
  void OnInteractionStarted(
      const chromeos::assistant::AssistantInteractionMetadata& metadata)
      override {}
  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override {}
  void OnTtsStarted(bool due_to_error) override {}
  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override {}
  void OnTextResponse(const std::string& response) override {}
  void OnSuggestionsResponse(
      const std::vector<AssistantSuggestion>& suggestions) override {}
  void OnOpenUrlResponse(const GURL& url, bool in_background) override {}
  void OnOpenAppResponse(const AndroidAppInfo& app_info) override {}
  void OnWaitStarted() override {}

  mojo::PendingRemote<chromeos::libassistant::mojom::ConversationObserver>
  BindNewPipeAndPassRemote();

 protected:
  ConversationObserver();
  ~ConversationObserver() override;

 private:
  mojo::Receiver<chromeos::libassistant::mojom::ConversationObserver>
      remote_observer_{this};
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_CONVERSATION_OBSERVER_H_
