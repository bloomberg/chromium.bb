// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_ACCOUNT_HOVER_LIST_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_ACCOUNT_HOVER_LIST_MODEL_H_

#include <stddef.h>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webauthn/hover_list_model.h"

namespace device {
class PublicKeyCredentialUserEntity;
}

class AccountHoverListModel : public HoverListModel {
 public:
  // Interface that the client should implement to learn when the user clicks on
  // views that observe the model.
  class Delegate {
   public:
    virtual void OnItemSelected(int index) = 0;
  };

  AccountHoverListModel(
      const std::vector<device::PublicKeyCredentialUserEntity>* users_list,
      Delegate* delegate);

  AccountHoverListModel(const AccountHoverListModel&) = delete;
  AccountHoverListModel& operator=(const AccountHoverListModel&) = delete;

  ~AccountHoverListModel() override;

  // HoverListModel:
  bool ShouldShowPlaceholderForEmptyList() const override;
  std::u16string GetPlaceholderText() const override;
  const gfx::VectorIcon* GetPlaceholderIcon() const override;
  std::vector<int> GetThrobberTags() const override;
  std::vector<int> GetButtonTags() const override;
  std::u16string GetItemText(int item_tag) const override;
  std::u16string GetDescriptionText(int item_tag) const override;
  const gfx::VectorIcon* GetItemIcon(int item_tag) const override;
  void OnListItemSelected(int item_tag) override;
  size_t GetPreferredItemCount() const override;
  bool StyleForTwoLines() const override;

 private:
  raw_ptr<const std::vector<device::PublicKeyCredentialUserEntity>> users_list_;
  const raw_ptr<Delegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_ACCOUNT_HOVER_LIST_MODEL_H_
