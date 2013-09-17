// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notifier_settings_view.h"

#include <set>
#include <string>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/message_center_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace message_center {
namespace {
const int kButtonPainterInsets = 5;
// We really want the margin to be 20px, but various views are padded by
// whitespace.
const int kDesiredMargin = 20;
// The MenuButton has 2px whitespace built-in.
const int kMenuButtonInnateMargin = 2;
const int kMinimumHorizontalMargin = kDesiredMargin - kMenuButtonInnateMargin;
// The EntryViews' leftmost view is a checkbox with 1px whitespace built in, so
// the margin for entry views should be one less than the target margin.
const int kCheckboxInnateMargin = 1;
const int kEntryMargin = kDesiredMargin - kCheckboxInnateMargin;
const int kMenuButtonLeftPadding = 12;
const int kMenuButtonRightPadding = 13;
const int kMenuButtonVerticalPadding = 9;
const int kMenuWhitespaceOffset = 2;
const int kMinimumWindowHeight = 480;
const int kMinimumWindowWidth = 320;
const int kSettingsTitleBottomMargin = 12;
const int kSettingsTitleTopMargin = 15;
const int kSpaceInButtonComponents = 16;
const int kTitleVerticalMargin = 1;
const int kTitleElementSpacing = 10;
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
  int content_width = width() - kEntryMargin * 2;
  int content_height = content->GetHeightForWidth(content_width);
  int y = std::max((height() - content_height) / 2, 0);
  content->SetBounds(kEntryMargin, y, content_width, content_height);
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

// NotifierGroupMenuButtonBorder ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class NotifierGroupMenuButtonBorder : public views::TextButtonDefaultBorder {
 public:
  NotifierGroupMenuButtonBorder();

 private:
  virtual ~NotifierGroupMenuButtonBorder();
};

NotifierGroupMenuButtonBorder::NotifierGroupMenuButtonBorder()
    : views::TextButtonDefaultBorder() {
  ui::ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  gfx::Insets insets(kButtonPainterInsets,
                     kButtonPainterInsets,
                     kButtonPainterInsets,
                     kButtonPainterInsets);

  set_normal_painter(views::Painter::CreateImagePainter(
      *rb.GetImageSkiaNamed(IDR_BUTTON_NORMAL), insets));
  set_hot_painter(views::Painter::CreateImagePainter(
      *rb.GetImageSkiaNamed(IDR_BUTTON_HOVER), insets));
  set_pushed_painter(views::Painter::CreateImagePainter(
      *rb.GetImageSkiaNamed(IDR_BUTTON_PRESSED), insets));

  SetInsets(gfx::Insets(kMenuButtonVerticalPadding,
                        kMenuButtonLeftPadding,
                        kMenuButtonVerticalPadding,
                        kMenuButtonRightPadding));
}

NotifierGroupMenuButtonBorder::~NotifierGroupMenuButtonBorder() {}

// NotifierGroupMenuModel //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class NotifierGroupMenuModel : public ui::SimpleMenuModel,
                               public ui::SimpleMenuModel::Delegate {
 public:
  NotifierGroupMenuModel(NotifierSettingsProvider* notifier_settings_provider);
  virtual ~NotifierGroupMenuModel();

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  NotifierSettingsProvider* notifier_settings_provider_;
};

NotifierGroupMenuModel::NotifierGroupMenuModel(
    NotifierSettingsProvider* notifier_settings_provider)
    : ui::SimpleMenuModel(this),
      notifier_settings_provider_(notifier_settings_provider) {
  if (!notifier_settings_provider_)
    return;

  size_t num_menu_items = notifier_settings_provider_->GetNotifierGroupCount();
  for (size_t i = 0; i < num_menu_items; ++i) {
    const NotifierGroup& group =
        notifier_settings_provider_->GetNotifierGroupAt(i);

    AddCheckItem(i, group.login_info.empty() ? group.name : group.login_info);
  }
}

NotifierGroupMenuModel::~NotifierGroupMenuModel() {}

bool NotifierGroupMenuModel::IsCommandIdChecked(int command_id) const {
  // If there's no provider, assume only one notifier group - the active one.
  if (!notifier_settings_provider_)
    return true;

  return notifier_settings_provider_->IsNotifierGroupActiveAt(command_id);
}

bool NotifierGroupMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool NotifierGroupMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void NotifierGroupMenuModel::ExecuteCommand(int command_id, int event_flags) {
  if (!notifier_settings_provider_)
    return;

  size_t notifier_group_index = static_cast<size_t>(command_id);
  size_t num_notifier_groups =
      notifier_settings_provider_->GetNotifierGroupCount();
  if (notifier_group_index >= num_notifier_groups)
    return;

  notifier_settings_provider_->SwitchToNotifierGroup(notifier_group_index);
}

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
    : title_arrow_(NULL),
      title_label_(NULL),
      notifier_group_selector_(NULL),
      scroller_(NULL),
      provider_(provider) {
  // |provider_| may be NULL in tests.
  if (provider_)
    provider_->AddObserver(this);

  set_focusable(true);
  set_focus_border(NULL);
  set_background(views::Background::CreateSolidBackground(
      kMessageCenterBackgroundColor));
  if (get_use_acceleration_when_possible())
    SetPaintToLayer(true);

  gfx::Font title_font =
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont);
  title_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_SETTINGS_BUTTON_LABEL),
      title_font);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetMultiLine(true);
  title_label_->set_border(
      views::Border::CreateEmptyBorder(kSettingsTitleTopMargin,
                                       kDesiredMargin,
                                       kSettingsTitleBottomMargin,
                                       kDesiredMargin));

  AddChildView(title_label_);

  scroller_ = new views::ScrollView();
  scroller_->SetVerticalScrollBar(new views::OverlayScrollBar(false));
  AddChildView(scroller_);

  std::vector<Notifier*> notifiers;
  if (provider_)
    provider_->GetNotifierList(&notifiers);

  UpdateContentsView(notifiers);
}

NotifierSettingsView::~NotifierSettingsView() {
  // |provider_| may be NULL in tests.
  if (provider_)
    provider_->RemoveObserver(this);
}

bool NotifierSettingsView::IsScrollable() {
  return scroller_->height() < scroller_->contents()->height();
}

void NotifierSettingsView::UpdateIconImage(const NotifierId& notifier_id,
                                           const gfx::Image& icon) {
  for (std::set<NotifierButton*>::iterator iter = buttons_.begin();
       iter != buttons_.end(); ++iter) {
    if ((*iter)->notifier().notifier_id == notifier_id) {
      (*iter)->UpdateIconImage(icon);
      return;
    }
  }
}

void NotifierSettingsView::NotifierGroupChanged() {
  std::vector<Notifier*> notifiers;
  if (provider_)
    provider_->GetNotifierList(&notifiers);

  UpdateContentsView(notifiers);
}

void NotifierSettingsView::UpdateContentsView(
    const std::vector<Notifier*>& notifiers) {
  buttons_.clear();

  views::View* contents_view = new views::View();
  contents_view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  views::View* contents_title_view = new views::View();
  contents_title_view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical,
                           kMinimumHorizontalMargin,
                           kTitleVerticalMargin,
                           kTitleElementSpacing));

  bool need_account_switcher =
      provider_ && provider_->GetNotifierGroupCount() > 1;
  int top_label_resource_id =
      need_account_switcher ? IDS_MESSAGE_CENTER_SETTINGS_DESCRIPTION_MULTIUSER
                            : IDS_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION;

  views::Label* top_label =
      new views::Label(l10n_util::GetStringUTF16(top_label_resource_id));

  top_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  top_label->SetMultiLine(true);
  top_label->set_border(views::Border::CreateEmptyBorder(
      0, kMenuButtonInnateMargin, 0, kMenuButtonInnateMargin));
  contents_title_view->AddChildView(top_label);

  string16 notifier_group_text;
  if (provider_) {
    const NotifierGroup& active_group = provider_->GetActiveNotifierGroup();
    notifier_group_text = active_group.login_info.empty()
                              ? active_group.name
                              : active_group.login_info;
  }

  if (need_account_switcher) {
    notifier_group_selector_ =
        new views::MenuButton(NULL, notifier_group_text, this, true);
    notifier_group_selector_->set_border(new NotifierGroupMenuButtonBorder);
    notifier_group_selector_->set_focus_border(NULL);
    notifier_group_selector_->set_animate_on_state_change(false);
    notifier_group_selector_->set_focusable(true);
    contents_title_view->AddChildView(notifier_group_selector_);
  }

  contents_view->AddChildView(contents_title_view);

  for (size_t i = 0; i < notifiers.size(); ++i) {
    NotifierButton* button = new NotifierButton(notifiers[i], this);
    EntryView* entry = new EntryView(button);
    entry->set_focusable(true);
    contents_view->AddChildView(entry);
    buttons_.insert(button);
  }

  scroller_->SetContents(contents_view);

  contents_view->SetBoundsRect(gfx::Rect(contents_view->GetPreferredSize()));
  InvalidateLayout();
}

void NotifierSettingsView::Layout() {
  int title_height = title_label_->GetHeightForWidth(width());
  title_label_->SetBounds(0, 0, width(), title_height);

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
  int total_height = title_label_->GetPreferredSize().height() +
                     scroller_->contents()->GetPreferredSize().height();
  if (total_height > kMinimumWindowHeight)
    size.Enlarge(scroller_->GetScrollBarWidth(), 0);
  return size;
}

gfx::Size NotifierSettingsView::GetPreferredSize() {
  gfx::Size preferred_size;
  gfx::Size title_size = title_label_->GetPreferredSize();
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

  if (iter == buttons_.end())
    return;

  (*iter)->SetChecked(!(*iter)->checked());
  if (provider_)
    provider_->SetNotifierEnabled((*iter)->notifier(), (*iter)->checked());
}

void NotifierSettingsView::OnMenuButtonClicked(views::View* source,
                                               const gfx::Point& point) {
  notifier_group_menu_model_.reset(new NotifierGroupMenuModel(provider_));
  notifier_group_menu_runner_.reset(
      new views::MenuRunner(notifier_group_menu_model_.get()));
  gfx::Rect menu_anchor = source->GetBoundsInScreen();
  menu_anchor.Inset(
      gfx::Insets(0, kMenuWhitespaceOffset, 0, kMenuWhitespaceOffset));
  if (views::MenuRunner::MENU_DELETED ==
      notifier_group_menu_runner_->RunMenuAt(GetWidget(),
                                             notifier_group_selector_,
                                             menu_anchor,
                                             views::MenuItemView::BUBBLE_ABOVE,
                                             ui::MENU_SOURCE_MOUSE,
                                             views::MenuRunner::CONTEXT_MENU))
    return;
  MessageCenterView* center_view = static_cast<MessageCenterView*>(parent());
  center_view->OnSettingsChanged();
}

}  // namespace message_center
