// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notifier_settings.h"

#include "grit/ui_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace message_center {
namespace {

const int kSpaceInButtonComponents = 16;
const int kMarginWidth = 16;

}  // namespace

// We do not use views::Checkbox class directly because it doesn't support
// showing 'icon'.
class NotifierSettingsView::NotifierButton : public views::CustomButton,
                                             public views::ButtonListener {
 public:
  NotifierButton(const NotifierSettingsView::Notifier& notifier,
                 views::ButtonListener* listener)
      : views::CustomButton(listener),
        notifier_(notifier),
        icon_view_(NULL),
        checkbox_(new views::Checkbox(string16())) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, kSpaceInButtonComponents));
    checkbox_->SetChecked(notifier.enabled);
    checkbox_->set_listener(this);
    AddChildView(checkbox_);
    UpdateIconImage(notifier_.icon);
    AddChildView(new views::Label(notifier.name));
  }

  void UpdateIconImage(gfx::ImageSkia icon) {
    notifier_.icon = icon;
    if (icon.isNull()) {
      delete icon_view_;
      icon_view_ = NULL;
    } else {
      if (!icon_view_) {
        icon_view_ = new views::ImageView();
        AddChildViewAt(icon_view_, 1);
      }
      icon_view_->SetImage(icon);
      icon_view_->SetImageSize(gfx::Size(kSettingsIconSize, kSettingsIconSize));
    }
    SchedulePaint();
  }

  void SetChecked(bool checked) {
    checkbox_->SetChecked(checked);
    notifier_.enabled = checked;
  }

  bool checked() const {
    return checkbox_->checked();
  }

  std::string id() const {
    return notifier_.id;
  }

 private:
  void ButtonPressed(views::Button* button, const ui::Event& event) {
    DCHECK(button == checkbox_);
    // The checkbox state has already changed at this point, but we'll update
    // the state on NotifierSettingsView::ButtonPressed() too, so here change
    // back to the previous state.
    checkbox_->SetChecked(!checkbox_->checked());
    CustomButton::NotifyClick(event);
  }

  NotifierSettingsView::Notifier notifier_;
  views::ImageView* icon_view_;
  views::Checkbox* checkbox_;

  DISALLOW_COPY_AND_ASSIGN(NotifierButton);
};

NotifierSettingsView::Notifier::Notifier(
    const std::string& id,
    NotifierSettingsView::Notifier::NotifierType type,
    const string16& name)
    : id(id),
      name(name),
      enabled(true),
      type(type) {
}

// static
NotifierSettingsView* NotifierSettingsView::Create(Delegate* delegate) {
  NotifierSettingsView* view = new NotifierSettingsView(delegate);
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = view;
  widget->Init(params);
  widget->Show();

  return view;
}

void NotifierSettingsView::UpdateIconImage(const std::string& id,
                                           const gfx::ImageSkia& icon) {
  for (std::set<NotifierButton*>::iterator iter = buttons_.begin();
       iter != buttons_.end(); ++iter) {
    if ((*iter)->id() == id) {
      (*iter)->UpdateIconImage(icon);
      return;
    }
  }
}

NotifierSettingsView::NotifierSettingsView(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);

  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, kMarginWidth, kMarginWidth, kMarginWidth));
  set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));

  views::Label* top_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_FOOTER_TITLE));
  top_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(top_label);

  views::View* items = new views::View();
  items->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, kMarginWidth));
  items->set_border(views::Border::CreateEmptyBorder(0, kMarginWidth, 0, 0));
  AddChildView(items);

  std::vector<Notifier> notifiers;
  delegate_->GetNotifierList(&notifiers);
  for (size_t i = 0; i < notifiers.size(); ++i) {
    NotifierButton* button = new NotifierButton(notifiers[i], this);
    items->AddChildView(button);
    buttons_.insert(button);
  }
}

NotifierSettingsView::~NotifierSettingsView() {
}

bool NotifierSettingsView::CanResize() const {
  return true;
}

string16 NotifierSettingsView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_SETTINGS_BUTTON_LABEL);
}

void NotifierSettingsView::WindowClosing() {
  delegate_->OnNotifierSettingsClosing(this);
}

views::View* NotifierSettingsView::GetContentsView() {
  return this;
}

void NotifierSettingsView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  std::set<NotifierButton*>::iterator iter = buttons_.find(
      static_cast<NotifierButton*>(sender));
  DCHECK(iter != buttons_.end());

  (*iter)->SetChecked(!(*iter)->checked());
  delegate_->SetNotifierEnabled((*iter)->id(), (*iter)->checked());
}

}  // namespace message_center
