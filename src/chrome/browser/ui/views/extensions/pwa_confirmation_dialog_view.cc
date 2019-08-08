// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/pwa_confirmation_dialog_view.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "url/origin.h"

PWAConfirmationDialogView::PWAConfirmationDialogView(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    chrome::AppInstallationAcceptanceCallback callback)
    : pwa_confirmation_(this, std::move(web_app_info), std::move(callback)) {}

PWAConfirmationDialogView::~PWAConfirmationDialogView() {}

gfx::Size PWAConfirmationDialogView::CalculatePreferredSize() const {
  int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);

  gfx::Size size = views::DialogDelegateView::CalculatePreferredSize();
  size.SetToMin(gfx::Size(bubble_width - margins().width(), size.height()));
  return size;
}

ui::ModalType PWAConfirmationDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 PWAConfirmationDialogView::GetWindowTitle() const {
  return PWAConfirmation::GetWindowTitle();
}

bool PWAConfirmationDialogView::ShouldShowCloseButton() const {
  return false;
}

views::View* PWAConfirmationDialogView::GetInitiallyFocusedView() {
  return PWAConfirmation::GetInitiallyFocusedView(this);
}

void PWAConfirmationDialogView::WindowClosing() {
  pwa_confirmation_.WindowClosing();
}

bool PWAConfirmationDialogView::Accept() {
  pwa_confirmation_.Accept();
  return true;
}

base::string16 PWAConfirmationDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return PWAConfirmation::GetDialogButtonLabel(button);
}

namespace chrome {

void ShowPWAInstallDialog(content::WebContents* web_contents,
                          std::unique_ptr<WebApplicationInfo> web_app_info,
                          AppInstallationAcceptanceCallback callback) {
  constrained_window::ShowWebModalDialogViews(
      new PWAConfirmationDialogView(std::move(web_app_info),
                                    std::move(callback)),
      web_contents);
}

}  // namespace chrome
