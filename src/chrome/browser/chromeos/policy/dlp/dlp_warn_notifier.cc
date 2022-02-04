// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_warn_notifier.h"

#include <memory>

#include "base/containers/cxx20_erase.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_dialog.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace policy {

DlpWarnNotifier::DlpWarnNotifier() = default;

DlpWarnNotifier::~DlpWarnNotifier() {
  for (views::Widget* widget : widgets_) {
    widget->RemoveObserver(this);
    widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

void DlpWarnNotifier::OnWidgetClosing(views::Widget* widget) {
  RemoveWidget(widget);
}

void DlpWarnNotifier::OnWidgetDestroying(views::Widget* widget) {
  RemoveWidget(widget);
}

void DlpWarnNotifier::ShowDlpPrintWarningDialog(
    OnDlpRestrictionCheckedCallback callback) {
  ShowDlpWarningDialog(std::move(callback),
                       DlpWarnDialog::DlpWarnDialogOptions(
                           DlpWarnDialog::Restriction::kPrinting));
}

void DlpWarnNotifier::ShowDlpScreenCaptureWarningDialog(
    OnDlpRestrictionCheckedCallback callback,
    const DlpConfidentialContents& confidential_contents) {
  ShowDlpWarningDialog(
      std::move(callback),
      DlpWarnDialog::DlpWarnDialogOptions(
          DlpWarnDialog::Restriction::kScreenCapture, confidential_contents));
}

void DlpWarnNotifier::ShowDlpVideoCaptureWarningDialog(
    OnDlpRestrictionCheckedCallback callback,
    const DlpConfidentialContents& confidential_contents) {
  ShowDlpWarningDialog(
      std::move(callback),
      DlpWarnDialog::DlpWarnDialogOptions(
          DlpWarnDialog::Restriction::kVideoCapture, confidential_contents));
}

void DlpWarnNotifier::ShowDlpScreenShareWarningDialog(
    OnDlpRestrictionCheckedCallback callback,
    const DlpConfidentialContents& confidential_contents,
    const std::u16string& application_title) {
  ShowDlpWarningDialog(std::move(callback),
                       DlpWarnDialog::DlpWarnDialogOptions(
                           DlpWarnDialog::Restriction::kScreenShare,
                           confidential_contents, application_title));
}

int DlpWarnNotifier::ActiveWarningDialogsCountForTesting() const {
  return widgets_.size();
}

void DlpWarnNotifier::ShowDlpWarningDialog(
    OnDlpRestrictionCheckedCallback callback,
    DlpWarnDialog::DlpWarnDialogOptions options) {
  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      new DlpWarnDialog(std::move(callback), options),
      /*context=*/nullptr, /*parent=*/nullptr);
  widget->Show();
  // We disable the dialog's hide animations after showing it so that it doesn't
  // end up showing in the screenshots, video recording, or screen share.
  widget->GetNativeWindow()->SetProperty(aura::client::kAnimationsDisabledKey,
                                         true);
  widget->AddObserver(this);
  widgets_.push_back(widget);
}

void DlpWarnNotifier::RemoveWidget(views::Widget* widget) {
  base::EraseIf(widgets_, [=](views::Widget* widget_ptr) -> bool {
    return widget_ptr == widget;
  });
}

}  // namespace policy
