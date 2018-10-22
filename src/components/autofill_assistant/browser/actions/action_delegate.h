// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"

namespace autofill_assistant {
struct ClientMemory;

// Action delegate called when processing actions.
class ActionDelegate {
 public:
  virtual ~ActionDelegate() = default;

  // Show status message on the bottom bar.
  virtual void ShowStatusMessage(const std::string& message) = 0;

  // Click the element given by |selectors| on the web page.
  virtual void ClickElement(const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Check whether the element given by |selectors| exists on the web page.
  virtual void ElementExists(const std::vector<std::string>& selectors,
                             base::OnceCallback<void(bool)> callback) = 0;

  // Ask user to choose an address in personal data manager. GUID of the chosen
  // address will be returned through callback if succeed, otherwise empty
  // string is returned.
  virtual void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Fill the address form given by |selectors| with the given address |guid| in
  // personal data manager.
  virtual void FillAddressForm(const std::string& guid,
                               const std::vector<std::string>& selectors,
                               base::OnceCallback<void(bool)> callback) = 0;

  // Ask user to choose a card in personal data manager. GUID of the chosen card
  // will be returned through callback if succeed, otherwise empty string is
  // returned.
  virtual void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Fill the card form given by |selectors| with the given card |guid| in
  // personal data manager.
  virtual void FillCardForm(const std::string& guid,
                            const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Return the current ClientMemory.
  virtual ClientMemory* GetClientMemory() = 0;

 protected:
  ActionDelegate() = default;
};
}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_
