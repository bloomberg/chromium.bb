// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"

#include <memory>

#include "base/strings/string16.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/front_eliding_title_label.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

PermissionPromptBubbleView::PermissionPromptBubbleView(
    Browser* browser,
    PermissionPrompt::Delegate* delegate)
    : browser_(browser),
      delegate_(delegate),
      name_or_origin_(delegate->GetDisplayNameOrOrigin()) {
  DCHECK(browser_);
  DCHECK(delegate_);

  // To prevent permissions being accepted accidentally, and as a security
  // measure against crbug.com/619429, permission prompts should not be accepted
  // as the default action.
  DialogDelegate::set_default_button(ui::DIALOG_BUTTON_NONE);

  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_CANCEL, l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));
  set_close_on_deactivate(false);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  for (PermissionRequest* request : delegate_->Requests())
    AddPermissionRequestLine(request);

  Show();

  chrome::RecordDialogCreation(chrome::DialogIdentifier::PERMISSIONS);
}

void PermissionPromptBubbleView::AddPermissionRequestLine(
    PermissionRequest* request) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  auto* line_container = AddChildView(std::make_unique<views::View>());
  line_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, provider->GetDistanceMetric(
                         DISTANCE_SUBSECTION_HORIZONTAL_INDENT)),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL)));

  auto* icon =
      line_container->AddChildView(std::make_unique<views::ImageView>());
  const gfx::VectorIcon& vector_id = request->GetIconId();
  const SkColor icon_color = icon->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DefaultIconColor);
  constexpr int kPermissionIconSize = 18;
  icon->SetImage(
      gfx::CreateVectorIcon(vector_id, kPermissionIconSize, icon_color));

  auto* label = line_container->AddChildView(
      std::make_unique<views::Label>(request->GetMessageTextFragment()));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

void PermissionPromptBubbleView::UpdateAnchorPosition() {
  DCHECK(browser_->window());

  set_parent_window(
      platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));

  bubble_anchor_util::AnchorConfiguration configuration =
      bubble_anchor_util::GetPageInfoAnchorConfiguration(browser_);
  SetAnchorView(configuration.anchor_view);
  SetHighlightedButton(configuration.highlighted_button);
  if (!configuration.anchor_view)
    SetAnchorRect(bubble_anchor_util::GetPageInfoAnchorRect(browser_));
  SetArrow(configuration.bubble_arrow);
}

void PermissionPromptBubbleView::CloseWithoutNotifyingDelegate() {
  notify_delegate_ = false;
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

gfx::NativeWindow PermissionPromptBubbleView::GetNativeWindow() {
  views::Widget* widget = GetWidget();
  return widget ? widget->GetNativeWindow() : nullptr;
}

void PermissionPromptBubbleView::AddedToWidget() {
  if (name_or_origin_.is_origin) {
    // There is a risk of URL spoofing from origins that are too wide to fit in
    // the bubble; elide origins from the front to prevent this.
    GetBubbleFrameView()->SetTitleView(
        CreateFrontElidingTitleLabel(GetWindowTitle()));
  }
}

bool PermissionPromptBubbleView::ShouldShowCloseButton() const {
  return true;
}

base::string16 PermissionPromptBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT,
                                    name_or_origin_.name_or_origin);
}

bool PermissionPromptBubbleView::Cancel() {
  if (notify_delegate_)
    delegate_->Deny();
  return true;
}

bool PermissionPromptBubbleView::Accept() {
  if (notify_delegate_)
    delegate_->Accept();
  return true;
}

bool PermissionPromptBubbleView::Close() {
  if (notify_delegate_)
    delegate_->Closing();
  return true;
}

void PermissionPromptBubbleView::Show() {
  DCHECK(browser_->window());

  // Set |parent_window| because some valid anchors can become hidden.
  set_parent_window(
      platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);
  // If a browser window (or popup) other than the bubble parent has focus,
  // don't take focus.
  if (browser_->window()->IsActive())
    widget->Show();
  else
    widget->ShowInactive();

  SizeToContents();
  UpdateAnchorPosition();
}
