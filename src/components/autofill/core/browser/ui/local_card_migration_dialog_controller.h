// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/legal_message_line.h"

namespace autofill {

enum class LocalCardMigrationDialogState;
class MigratableCreditCard;

// TODO(crbug.com/867194): Add legal message.
// Interface that exposes controller functionality to local card migration
// dialog views.
class LocalCardMigrationDialogController {
 public:
  LocalCardMigrationDialogController() {}
  virtual ~LocalCardMigrationDialogController() {}

  virtual LocalCardMigrationDialogState GetViewState() const = 0;
  virtual void SetViewState(LocalCardMigrationDialogState view_state) = 0;
  virtual const std::vector<MigratableCreditCard>& GetCardList() const = 0;
  // TODO(crbug.com/867194): Ensure this would not be called when migration is
  // happending.
  virtual void SetCardList(std::vector<MigratableCreditCard>& card_list) = 0;
  virtual const LegalMessageLines& GetLegalMessageLines() const = 0;
  virtual void OnCardSelected(int index) = 0;
  virtual void OnDialogClosed() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationDialogController);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_H_
