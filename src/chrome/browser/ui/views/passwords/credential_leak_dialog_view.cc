// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/credential_leak_dialog_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// Updates the image displayed on the illustration based on the current theme.
void UpdateImageView(NonAccessibleImageView* image_view,
                     bool dark_mode_enabled) {
  image_view->SetImage(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          dark_mode_enabled ? IDR_PASSWORD_CHECK_DARK : IDR_PASSWORD_CHECK));
}

// Creates the illustration which is rendered on top of the dialog.
std::unique_ptr<NonAccessibleImageView> CreateIllustration(
    bool dark_mode_enabled) {
  auto image_view = std::make_unique<NonAccessibleImageView>();
  UpdateImageView(image_view.get(), dark_mode_enabled);
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  return image_view;
}

std::unique_ptr<views::TooltipIcon> CreateInfoIcon() {
  auto explanation_tooltip = std::make_unique<views::TooltipIcon>(
      password_manager::GetLeakDetectionTooltip());
  explanation_tooltip->set_bubble_width(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BUBBLE_PREFERRED_WIDTH));
  explanation_tooltip->set_anchor_point_arrow(
      views::BubbleBorder::Arrow::TOP_RIGHT);
  return explanation_tooltip;
}

}  // namespace

CredentialLeakDialogView::CredentialLeakDialogView(
    CredentialLeakDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  DCHECK(controller);
  DCHECK(web_contents);

  SetButtons(controller->ShouldShowCancelButton()
                 ? ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL
                 : ui::DIALOG_BUTTON_OK);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller_->GetAcceptButtonLabel());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, controller_->GetCancelButtonLabel());

  using ControllerClosureFn = void (CredentialLeakDialogController::*)(void);
  auto close_callback = [](CredentialLeakDialogController** controller,
                           ControllerClosureFn fn) {
    // Null out the controller pointer stored in the parent object, to avoid any
    // further calls to the controller and inhibit recursive closes that would
    // otherwise happen in ControllerGone(), and invoke the provided method on
    // the controller.
    //
    // Note that when this lambda gets bound it closes over &controller_, not
    // controller_ itself!
    (std::exchange(*controller, nullptr)->*(fn))();
  };

  SetAcceptCallback(
      base::BindOnce(close_callback, base::Unretained(&controller_),
                     &CredentialLeakDialogController::OnAcceptDialog));
  SetCancelCallback(
      base::BindOnce(close_callback, base::Unretained(&controller_),
                     &CredentialLeakDialogController::OnCancelDialog));
  SetCloseCallback(
      base::BindOnce(close_callback, base::Unretained(&controller_),
                     &CredentialLeakDialogController::OnCloseDialog));
}

CredentialLeakDialogView::~CredentialLeakDialogView() = default;

void CredentialLeakDialogView::ShowCredentialLeakPrompt() {
  InitWindow();
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void CredentialLeakDialogView::ControllerGone() {
  // Widget::Close() synchronously calls Close() on this instance, which resets
  // the |controller_|. The null check for |controller_| here is to avoid
  // reentry into Close() - |controller_| might have been nulled out by the
  // closure callbacks already, in which case the dialog is already closing. See
  // the definition of |close_callback| in the constructor.
  if (controller_)
    GetWidget()->Close();
}

ui::ModalType CredentialLeakDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

gfx::Size CredentialLeakDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

bool CredentialLeakDialogView::ShouldShowCloseButton() const {
  return false;
}

void CredentialLeakDialogView::OnThemeChanged() {
  views::DialogDelegateView::OnThemeChanged();
  GetBubbleFrameView()->SetHeaderView(
      CreateIllustration(GetNativeTheme()->ShouldUseDarkColors()));
}

base::string16 CredentialLeakDialogView::GetWindowTitle() const {
  // |controller_| can be nullptr when the framework calls this method after a
  // button click.
  return controller_ ? controller_->GetTitle() : base::string16();
}

void CredentialLeakDialogView::InitWindow() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::CONTROL, views::CONTROL)));

  auto description_label = std::make_unique<views::Label>(
      controller_->GetDescription(), views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY);
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(description_label));
  SetExtraView(CreateInfoIcon());
}

CredentialLeakPrompt* CreateCredentialLeakPromptView(
    CredentialLeakDialogController* controller,
    content::WebContents* web_contents) {
  return new CredentialLeakDialogView(controller, web_contents);
}
