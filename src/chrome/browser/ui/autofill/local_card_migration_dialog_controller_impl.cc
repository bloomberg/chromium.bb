// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/local_card_migration_dialog_controller_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_state.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"

// TODO(crbug.com/867194): Add time counter for showing 'close' button if
// uploading takes too long.

namespace autofill {

LocalCardMigrationDialogControllerImpl::LocalCardMigrationDialogControllerImpl(
    content::WebContents* web_contents)
    : local_card_migration_dialog_(nullptr) {}

LocalCardMigrationDialogControllerImpl::
    ~LocalCardMigrationDialogControllerImpl() {
  if (local_card_migration_dialog_)
    local_card_migration_dialog_->CloseDialog();
}

void LocalCardMigrationDialogControllerImpl::ShowDialog(
    std::unique_ptr<base::DictionaryValue> legal_message,
    LocalCardMigrationDialog* local_card_migration_dialog,
    base::OnceClosure user_accepted_migration_closure) {
  if (local_card_migration_dialog_)
    local_card_migration_dialog_->CloseDialog();

  if (!LegalMessageLine::Parse(*legal_message, &legal_message_lines_,
                               /*escape_apostrophes=*/true)) {
    // TODO(crbug/867194): Add metric.
    return;
  }

  local_card_migration_dialog_ = local_card_migration_dialog;
  local_card_migration_dialog_->ShowDialog(
      std::move(user_accepted_migration_closure));
}

LocalCardMigrationDialogState
LocalCardMigrationDialogControllerImpl::GetViewState() const {
  return view_state_;
}

void LocalCardMigrationDialogControllerImpl::SetViewState(
    LocalCardMigrationDialogState view_state) {
  view_state_ = view_state;
}

const std::vector<MigratableCreditCard>&
LocalCardMigrationDialogControllerImpl::GetCardList() const {
  return migratable_credit_cards_;
}

void LocalCardMigrationDialogControllerImpl::SetCardList(
    std::vector<MigratableCreditCard>& migratable_credit_cards) {
  migratable_credit_cards_ = migratable_credit_cards;
}

const LegalMessageLines&
LocalCardMigrationDialogControllerImpl::GetLegalMessageLines() const {
  return legal_message_lines_;
}

void LocalCardMigrationDialogControllerImpl::OnCardSelected(int index) {
  migratable_credit_cards_[index].ToggleChosen();
}

void LocalCardMigrationDialogControllerImpl::OnDialogClosed() {
  if (local_card_migration_dialog_)
    local_card_migration_dialog_ = nullptr;
}

}  // namespace autofill