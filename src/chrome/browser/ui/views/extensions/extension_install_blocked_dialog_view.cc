// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_install_blocked_dialog_view.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace chrome {

void ShowExtensionInstallBlockedDialog(
    const std::string& extension_name,
    const base::string16& custom_error_message,
    const gfx::ImageSkia& icon,
    content::WebContents* web_contents,
    base::OnceClosure done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto dialog = std::make_unique<ExtensionInstallBlockedDialogView>(
      extension_name, custom_error_message, icon, std::move(done_callback));
  constrained_window::ShowWebModalDialogViews(dialog.release(), web_contents)
      ->Show();
}

}  // namespace chrome

ExtensionInstallBlockedDialogView::ExtensionInstallBlockedDialogView(
    const std::string& extension_name,
    const base::string16& custom_error_message,
    const gfx::ImageSkia& icon,
    base::OnceClosure done_callback)
    : title_(l10n_util::GetStringFUTF16(
          IDS_EXTENSION_BLOCKED_BY_POLICY_PROMPT_TITLE,
          base::UTF8ToUTF16(extension_name))),
      custom_error_message_(custom_error_message),
      icon_(gfx::ImageSkiaOperations::CreateResizedImage(
          icon,
          skia::ImageOperations::ResizeMethod::RESIZE_BEST,
          gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                    extension_misc::EXTENSION_ICON_SMALL))),
      done_callback_(std::move(done_callback)) {
  SetButtons(ui::DIALOG_BUTTON_CANCEL);
  SetDefaultButton(ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(IDS_CLOSE));
  set_draggable(true);
  set_close_on_deactivate(false);
  AddCustomMessageContents();
}

ExtensionInstallBlockedDialogView::~ExtensionInstallBlockedDialogView() {
  if (done_callback_)
    std::move(done_callback_).Run();
}

gfx::Size ExtensionInstallBlockedDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

bool ExtensionInstallBlockedDialogView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
  return true;
}

base::string16 ExtensionInstallBlockedDialogView::GetWindowTitle() const {
  return title_;
}

bool ExtensionInstallBlockedDialogView::ShouldShowWindowIcon() const {
  return true;
}

gfx::ImageSkia ExtensionInstallBlockedDialogView::GetWindowIcon() {
  return icon_;
}

ui::ModalType ExtensionInstallBlockedDialogView::GetModalType() const {
  // Make sure user know the installation is blocked before taking further
  // action.
  return ui::MODAL_TYPE_CHILD;
}

void ExtensionInstallBlockedDialogView::AddCustomMessageContents() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  if (custom_error_message_.empty())
    return;

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  auto extension_info_container = std::make_unique<views::View>();
  const gfx::Insets content_insets =
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT);
  extension_info_container->SetBorder(views::CreateEmptyBorder(
      0, content_insets.left(), 0, content_insets.right()));
  extension_info_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  const int content_width = GetPreferredSize().width() -
                            extension_info_container->GetInsets().width();

  set_margins(gfx::Insets(content_insets.top(), 0, content_insets.bottom(), 0));

  auto* header_label =
      extension_info_container->AddChildView(std::make_unique<views::Label>(
          custom_error_message_, CONTEXT_BODY_TEXT_LARGE));
  header_label->SetMultiLine(true);
  header_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  header_label->SizeToFit(content_width);

  auto* scroll_view = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->SetHideHorizontalScrollBar(true);
  scroll_view->SetContents(std::move(extension_info_container));
  scroll_view->ClipHeightTo(
      0, provider->GetDistanceMetric(
             views::DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT));
}
