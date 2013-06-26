// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notifier_settings_view.h"

#include <set>
#include <string>

#include "base/strings/string16.h"
#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/message_center_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace message_center {
namespace {

const int kSpaceInButtonComponents = 16;
const int kMarginWidth = 16;
const int kMinimumWindowWidth = 320;
const int kMinimumWindowHeight = 480;
const int kEntryHeight = kMinimumWindowHeight / 10;

// The view to guarantee the 48px height and place the contents at the
// middle. It also guarantee the left margin.
class EntryView : public views::View {
 public:
  EntryView(views::View* contents);
  virtual ~EntryView();

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const ui::KeyEvent& event) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(EntryView);
};

EntryView::EntryView(views::View* contents) {
  AddChildView(contents);
}

EntryView::~EntryView() {
}

void EntryView::Layout() {
  DCHECK_EQ(1, child_count());
  views::View* content = child_at(0);
  int content_width = width() - kMarginWidth * 2;
  int content_height = content->GetHeightForWidth(content_width);
  int y = std::max((height() - content_height) / 2, 0);
  content->SetBounds(kMarginWidth, y, content_width, content_height);
}

gfx::Size EntryView::GetPreferredSize() {
  DCHECK_EQ(1, child_count());
  gfx::Size size = child_at(0)->GetPreferredSize();
  size.SetToMax(gfx::Size(kMinimumWindowWidth, kEntryHeight));
  return size;
}

void EntryView::GetAccessibleState(ui::AccessibleViewState* state) {
  DCHECK_EQ(1, child_count());
  child_at(0)->GetAccessibleState(state);
}

void EntryView::OnFocus() {
  views::View::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
}

void EntryView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
    canvas->DrawRect(gfx::Rect(2, 1, width() - 4, height() - 3),
                     kFocusBorderColor);
  }
}

bool EntryView::OnKeyPressed(const ui::KeyEvent& event) {
  return child_at(0)->OnKeyPressed(event);
}

bool EntryView::OnKeyReleased(const ui::KeyEvent& event) {
  return child_at(0)->OnKeyReleased(event);
}

}  // namespace

// We do not use views::Checkbox class directly because it doesn't support
// showing 'icon'.
class NotifierSettingsView::NotifierButton : public views::CustomButton,
                                             public views::ButtonListener {
 public:
  NotifierButton(Notifier* notifier, views::ButtonListener* listener)
      : views::CustomButton(listener),
        notifier_(notifier),
        icon_view_(NULL),
        checkbox_(new views::Checkbox(string16())) {
    DCHECK(notifier);
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, kSpaceInButtonComponents));
    checkbox_->SetChecked(notifier_->enabled);
    checkbox_->set_listener(this);
    checkbox_->set_focusable(false);
    checkbox_->SetAccessibleName(notifier_->name);
    AddChildView(checkbox_);
    UpdateIconImage(notifier_->icon);
    AddChildView(new views::Label(notifier_->name));
  }

  void UpdateIconImage(const gfx::Image& icon) {
    notifier_->icon = icon;
    if (icon.IsEmpty()) {
      delete icon_view_;
      icon_view_ = NULL;
    } else {
      if (!icon_view_) {
        icon_view_ = new views::ImageView();
        AddChildViewAt(icon_view_, 1);
      }
      icon_view_->SetImage(icon.ToImageSkia());
      icon_view_->SetImageSize(gfx::Size(kSettingsIconSize, kSettingsIconSize));
    }
    Layout();
    SchedulePaint();
  }

  void SetChecked(bool checked) {
    checkbox_->SetChecked(checked);
    notifier_->enabled = checked;
  }

  bool checked() const {
    return checkbox_->checked();
  }

  const Notifier& notifier() const {
    return *notifier_.get();
  }

 private:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* button,
                             const ui::Event& event) OVERRIDE {
    DCHECK(button == checkbox_);
    // The checkbox state has already changed at this point, but we'll update
    // the state on NotifierSettingsView::ButtonPressed() too, so here change
    // back to the previous state.
    checkbox_->SetChecked(!checkbox_->checked());
    CustomButton::NotifyClick(event);
  }

  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE {
    static_cast<views::View*>(checkbox_)->GetAccessibleState(state);
  }

  scoped_ptr<Notifier> notifier_;
  views::ImageView* icon_view_;
  views::Checkbox* checkbox_;

  DISALLOW_COPY_AND_ASSIGN(NotifierButton);
};

NotifierSettingsView::NotifierSettingsView(NotifierSettingsProvider* provider)
    : provider_(provider) {
  // |provider_| may be NULL in tests.
  if (provider_)
    provider_->AddObserver(this);

  set_focusable(true);
  set_focus_border(NULL);
  set_background(views::Background::CreateSolidBackground(
      kMessageCenterBackgroundColor));
  if (get_use_acceleration_when_possible())
    SetPaintToLayer(true);

  ResourceBundle& bundle = ResourceBundle::GetSharedInstance();

  views::View* title_container = new views::View;
  // The title_arrow and title_label aren't aligned well in Windows for the
  // horizontal BoxLayout. That's why GridLayout with vertical alignment is
  // used here.
  views::GridLayout* title_layout = new views::GridLayout(title_container);
  title_container->SetLayoutManager(title_layout);
  views::ColumnSet* columns = title_layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, kMarginWidth);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                     1, views::GridLayout::USE_PREF, 0, 0);
  title_arrow_ = new views::ImageButton(this);
  title_arrow_->SetImage(views::Button::STATE_NORMAL, bundle.GetImageSkiaNamed(
      IDR_NOTIFICATION_ARROW));
  title_arrow_->SetImage(views::Button::STATE_HOVERED, bundle.GetImageSkiaNamed(
      IDR_NOTIFICATION_ARROW_HOVER));
  title_arrow_->SetImage(views::Button::STATE_PRESSED, bundle.GetImageSkiaNamed(
      IDR_NOTIFICATION_ARROW_PRESSED));
  gfx::Font title_font =
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont);
  views::Label* title_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_SETTINGS_BUTTON_LABEL),
      title_font);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);
  title_layout->StartRow(0, 0);
  title_layout->AddView(title_arrow_);
  title_layout->AddView(title_label);
  title_entry_ = new EntryView(title_container);
  AddChildView(title_entry_);

  scroller_ = new views::ScrollView();
  scroller_->SetVerticalScrollBar(new views::OverlayScrollBar(false));
  AddChildView(scroller_);

  views::View* contents_view = new views::View();
  contents_view->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, 0));

  views::Label* top_label = new views::Label(l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION));
  top_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  top_label->SetMultiLine(true);
  top_label->SizeToFit(kMinimumWindowWidth - kMarginWidth * 2);
  contents_view->AddChildView(new EntryView(top_label));

  std::vector<Notifier*> notifiers;
  if (provider_)
    provider_->GetNotifierList(&notifiers);
  for (size_t i = 0; i < notifiers.size(); ++i) {
    NotifierButton* button = new NotifierButton(notifiers[i], this);
    EntryView* entry = new EntryView(button);
    entry->set_focusable(true);
    contents_view->AddChildView(entry);
    buttons_.insert(button);
  }
  scroller_->SetContents(contents_view);

  contents_view->SetBoundsRect(gfx::Rect(contents_view->GetPreferredSize()));
}

NotifierSettingsView::~NotifierSettingsView() {
  // |provider_| may be NULL in tests.
  if (provider_)
    provider_->RemoveObserver(this);
}

bool NotifierSettingsView::IsScrollable() {
  return scroller_->height() < scroller_->contents()->height();
}

void NotifierSettingsView::UpdateIconImage(const std::string& id,
                                           const gfx::Image& icon) {
  for (std::set<NotifierButton*>::iterator iter = buttons_.begin();
       iter != buttons_.end(); ++iter) {
    if ((*iter)->notifier().id == id) {
      (*iter)->UpdateIconImage(icon);
      return;
    }
  }
}

void NotifierSettingsView::UpdateFavicon(const GURL& url,
                                         const gfx::Image& icon) {
  for (std::set<NotifierButton*>::iterator iter = buttons_.begin();
       iter != buttons_.end(); ++iter) {
    if ((*iter)->notifier().url == url) {
      (*iter)->UpdateIconImage(icon);
      return;
    }
  }
}

void NotifierSettingsView::Layout() {
  int title_height = title_entry_->GetHeightForWidth(width());
  title_entry_->SetBounds(0, 0, width(), title_height);

  views::View* contents_view = scroller_->contents();
  int content_width = width();
  int content_height = contents_view->GetHeightForWidth(content_width);
  if (title_height + content_height > height()) {
    content_width -= scroller_->GetScrollBarWidth();
    content_height = contents_view->GetHeightForWidth(content_width);
  }
  contents_view->SetBounds(0, 0, content_width, content_height);
  scroller_->SetBounds(0, title_height, width(), height() - title_height);
}

gfx::Size NotifierSettingsView::GetMinimumSize() {
  gfx::Size size(kMinimumWindowWidth, kMinimumWindowHeight);
  int total_height = title_entry_->GetPreferredSize().height() +
      scroller_->contents()->GetPreferredSize().height();
  if (total_height > kMinimumWindowHeight)
    size.Enlarge(scroller_->GetScrollBarWidth(), 0);
  return size;
}

gfx::Size NotifierSettingsView::GetPreferredSize() {
  gfx::Size preferred_size;
  std::vector<gfx::Size> child_sizes;
  gfx::Size title_size = title_entry_->GetPreferredSize();
  gfx::Size content_size = scroller_->contents()->GetPreferredSize();
  return gfx::Size(std::max(title_size.width(), content_size.width()),
                   title_size.height() + content_size.height());
}

bool NotifierSettingsView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_ESCAPE) {
    GetWidget()->Close();
    return true;
  }

  return scroller_->OnKeyPressed(event);
}

bool NotifierSettingsView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  return scroller_->OnMouseWheel(event);
}

void NotifierSettingsView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  if (sender == title_arrow_) {
    MessageCenterView* center_view = static_cast<MessageCenterView*>(parent());
    center_view->SetSettingsVisible(!center_view->settings_visible());
    return;
  }

  std::set<NotifierButton*>::iterator iter = buttons_.find(
      static_cast<NotifierButton*>(sender));
  DCHECK(iter != buttons_.end());

  (*iter)->SetChecked(!(*iter)->checked());
  if (provider_)
    provider_->SetNotifierEnabled((*iter)->notifier(), (*iter)->checked());
}

}  // namespace message_center
