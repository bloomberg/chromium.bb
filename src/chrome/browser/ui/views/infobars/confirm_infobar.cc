// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/confirm_infobar.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/elevation_icon_setter.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/view_class_properties.h"

ConfirmInfoBar::ConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate> delegate)
    : InfoBarView(std::move(delegate)) {
  auto* delegate_ptr = GetDelegate();
  label_ = CreateLabel(delegate_ptr->GetMessageText());
  label_->SetElideBehavior(delegate_ptr->GetMessageElideBehavior());
  AddChildView(label_.get());

  const auto create_button = [this](ConfirmInfoBarDelegate::InfoBarButton type,
                                    void (ConfirmInfoBar::*click_function)()) {
    auto* button = AddChildView(std::make_unique<views::MdTextButton>(
        base::BindRepeating(click_function, base::Unretained(this)),
        GetDelegate()->GetButtonLabel(type)));
    button->SetProperty(
        views::kMarginsKey,
        gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_TOAST_CONTROL_VERTICAL),
                    0));
    return button;
  };

  const auto buttons = delegate_ptr->GetButtons();
  if (buttons & ConfirmInfoBarDelegate::BUTTON_OK) {
    ok_button_ = create_button(ConfirmInfoBarDelegate::BUTTON_OK,
                               &ConfirmInfoBar::OkButtonPressed);
    ok_button_->SetProminent(true);
    if (delegate_ptr->OKButtonTriggersUACPrompt()) {
      elevation_icon_setter_ = std::make_unique<ElevationIconSetter>(
          ok_button_,
          base::BindOnce(&ConfirmInfoBar::Layout, base::Unretained(this)));
    }
  }

  if (buttons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    cancel_button_ = create_button(ConfirmInfoBarDelegate::BUTTON_CANCEL,
                                   &ConfirmInfoBar::CancelButtonPressed);
    if (buttons == ConfirmInfoBarDelegate::BUTTON_CANCEL)
      cancel_button_->SetProminent(true);
    cancel_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        delegate_ptr->GetButtonImage(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  }

  link_ = CreateLink(delegate_ptr->GetLinkText());
  AddChildView(link_.get());
}

ConfirmInfoBar::~ConfirmInfoBar() {
  // Ensure |elevation_icon_setter_| is destroyed before |ok_button_|.
  elevation_icon_setter_.reset();
}

void ConfirmInfoBar::Layout() {
  InfoBarView::Layout();

  if (ok_button_) {
    ok_button_->SizeToPreferredSize();
  }

  if (cancel_button_) {
    cancel_button_->SizeToPreferredSize();
  }

  int x = GetStartX();
  Views views;
  views.push_back(label_);
  views.push_back(link_);
  AssignWidths(&views, std::max(0, GetEndX() - x - NonLabelWidth()));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  label_->SetPosition(gfx::Point(x, OffsetY(label_)));
  if (!label_->GetText().empty())
    x = label_->bounds().right() +
        layout_provider->GetDistanceMetric(
            views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  if (ok_button_) {
    ok_button_->SetPosition(gfx::Point(x, OffsetY(ok_button_)));
    x = ok_button_->bounds().right() +
        layout_provider->GetDistanceMetric(
            views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  }

  if (cancel_button_)
    cancel_button_->SetPosition(gfx::Point(x, OffsetY(cancel_button_)));

  link_->SetPosition(gfx::Point(GetEndX() - link_->width(), OffsetY(link_)));
}

void ConfirmInfoBar::OkButtonPressed() {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  if (GetDelegate()->Accept())
    RemoveSelf();
}

void ConfirmInfoBar::CancelButtonPressed() {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  if (GetDelegate()->Cancel())
    RemoveSelf();
}

ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

int ConfirmInfoBar::GetContentMinimumWidth() const {
  return label_->GetMinimumSize().width() + link_->GetMinimumSize().width() +
         NonLabelWidth();
}

int ConfirmInfoBar::NonLabelWidth() const {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  const int label_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int button_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  int width = (label_->GetText().empty() || (!ok_button_ && !cancel_button_))
                  ? 0
                  : label_spacing;
  if (ok_button_)
    width += ok_button_->width() + (cancel_button_ ? button_spacing : 0);
  width += cancel_button_ ? cancel_button_->width() : 0;
  return width + ((link_->GetText().empty() || !width) ? 0 : label_spacing);
}
