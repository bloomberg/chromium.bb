// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/remove_query_confirmation_dialog.h"

#include <memory>
#include <utility>

#include "ash/app_list/views/search_box_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace ash {

namespace {

constexpr int kDialogWidth = 320;

}  // namespace

RemoveQueryConfirmationDialog::RemoveQueryConfirmationDialog(
    const base::string16& query,
    RemovalConfirmationCallback confirm_callback)
    : confirm_callback_(std::move(confirm_callback)) {
  SetTitle(l10n_util::GetStringUTF16(IDS_REMOVE_ZERO_STATE_SUGGESTION_TITLE));
  SetShowCloseButton(false);

  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_REMOVE_SUGGESTION_BUTTON_LABEL));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(IDS_APP_CANCEL));

  auto run_callback = [](RemoveQueryConfirmationDialog* dialog, bool accept) {
    std::move(dialog->confirm_callback_).Run(accept);
  };
  SetAcceptCallback(base::BindOnce(run_callback, base::Unretained(this), true));
  SetCancelCallback(
      base::BindOnce(run_callback, base::Unretained(this), false));

  const views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  views::Label* label = new views::Label(l10n_util::GetStringFUTF16(
      IDS_REMOVE_ZERO_STATE_SUGGESTION_DETAILS, query));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  AddChildView(label);
}

RemoveQueryConfirmationDialog::~RemoveQueryConfirmationDialog() = default;

const char* RemoveQueryConfirmationDialog::GetClassName() const {
  return "RemoveQueryConfirmationDialog";
}

gfx::Size RemoveQueryConfirmationDialog::CalculatePreferredSize() const {
  const int default_width = kDialogWidth;
  return gfx::Size(default_width, GetHeightForWidth(default_width));
}

ui::ModalType RemoveQueryConfirmationDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

}  // namespace ash
