// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_package_install_failure_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"

namespace crostini {

// Implementation from crostini_util.h, necessary due to chrome's inclusion
// rules.
void ShowCrostiniPackageInstallFailureView(const std::string& error_message) {
  CrostiniPackageInstallFailureView::Show(error_message);
}

}  // namespace crostini

void CrostiniPackageInstallFailureView::Show(const std::string& error_message) {
  views::DialogDelegate::CreateDialogWidget(
      new CrostiniPackageInstallFailureView(error_message), nullptr, nullptr)
      ->Show();
}

bool CrostiniPackageInstallFailureView::ShouldShowCloseButton() const {
  return false;
}

base::string16 CrostiniPackageInstallFailureView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_CROSTINI_PACKAGE_INSTALL_FAILURE_VIEW_TITLE);
}

gfx::Size CrostiniPackageInstallFailureView::CalculatePreferredSize() const {
  const int dialog_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                           margins().width();
  return gfx::Size(dialog_width, GetHeightForWidth(dialog_width));
}

CrostiniPackageInstallFailureView::CrostiniPackageInstallFailureView(
    const std::string& error_message) {
  SetButtons(ui::DIALOG_BUTTON_OK);
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::TEXT, views::DialogContentType::TEXT));

  views::StyledLabel* message_label = new views::StyledLabel(
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_FAILURE_VIEW_MESSAGE),
      nullptr);
  AddChildView(message_label);

  views::MessageBoxView::InitParams error_box_init_params(
      base::UTF8ToUTF16(error_message));
  views::MessageBoxView* error_box =
      new views::MessageBoxView(error_box_init_params);
  AddChildView(error_box);

  set_close_on_deactivate(true);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::CROSTINI_FORCE_CLOSE);
}

CrostiniPackageInstallFailureView::~CrostiniPackageInstallFailureView() =
    default;
