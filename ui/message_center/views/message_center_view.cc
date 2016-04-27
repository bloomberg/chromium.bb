// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_center_view.h"

#include <list>
#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/views/message_center_button_bar.h"
#include "ui/message_center/views/message_list_view.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/message_view_context_menu_controller.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/message_center/views/notifier_settings_view.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace message_center {

namespace {

const SkColor kNoNotificationsTextColor = SkColorSetRGB(0xb4, 0xb4, 0xb4);
#if defined(OS_LINUX) && defined(OS_CHROMEOS)
const SkColor kTransparentColor = SkColorSetARGB(0, 0, 0, 0);
#endif

const int kDefaultAnimationDurationMs = 120;
const int kDefaultFrameRateHz = 60;

void SetViewHierarchyEnabled(views::View* view, bool enabled) {
  for (int i = 0; i < view->child_count(); i++)
    SetViewHierarchyEnabled(view->child_at(i), enabled);
  view->SetEnabled(enabled);
}

}  // namespace

class NoNotificationMessageView : public views::View {
 public:
  NoNotificationMessageView();
  ~NoNotificationMessageView() override;

  // Overridden from views::View.
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void Layout() override;

 private:
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(NoNotificationMessageView);
};

NoNotificationMessageView::NoNotificationMessageView() {
  label_ = new views::Label(l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_NO_MESSAGES));
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetEnabledColor(kNoNotificationsTextColor);
  // Set transparent background to ensure that subpixel rendering
  // is disabled. See crbug.com/169056
#if defined(OS_LINUX) && defined(OS_CHROMEOS)
  label_->SetBackgroundColor(kTransparentColor);
#endif
  AddChildView(label_);
}

NoNotificationMessageView::~NoNotificationMessageView() {
}

gfx::Size NoNotificationMessageView::GetPreferredSize() const {
  return gfx::Size(kMinScrollViewHeight, label_->GetPreferredSize().width());
}

int NoNotificationMessageView::GetHeightForWidth(int width) const {
  return kMinScrollViewHeight;
}

void NoNotificationMessageView::Layout() {
  int text_height = label_->GetHeightForWidth(width());
  int margin = (height() - text_height) / 2;
  label_->SetBounds(0, margin, width(), text_height);
}

// MessageCenterView ///////////////////////////////////////////////////////////

MessageCenterView::MessageCenterView(MessageCenter* message_center,
                                     MessageCenterTray* tray,
                                     int max_height,
                                     bool initially_settings_visible,
                                     bool top_down,
                                     const base::string16& title)
    : message_center_(message_center),
      tray_(tray),
      scroller_(NULL),
      settings_view_(NULL),
      button_bar_(NULL),
      top_down_(top_down),
      settings_visible_(initially_settings_visible),
      source_view_(NULL),
      source_height_(0),
      target_view_(NULL),
      target_height_(0),
      is_closing_(false),
      context_menu_controller_(new MessageViewContextMenuController(this)) {
  message_center_->AddObserver(this);
  set_notify_enter_exit_on_child(true);
  set_background(views::Background::CreateSolidBackground(
      kMessageCenterBackgroundColor));

  NotifierSettingsProvider* notifier_settings_provider =
      message_center_->GetNotifierSettingsProvider();
  button_bar_ = new MessageCenterButtonBar(this,
                                           message_center,
                                           notifier_settings_provider,
                                           initially_settings_visible,
                                           title);

  const int button_height = button_bar_->GetPreferredSize().height();

  scroller_ = new views::ScrollView();
  scroller_->ClipHeightTo(kMinScrollViewHeight, max_height - button_height);
  scroller_->SetVerticalScrollBar(new views::OverlayScrollBar(false));
  scroller_->set_background(
      views::Background::CreateSolidBackground(kMessageCenterBackgroundColor));

  scroller_->SetPaintToLayer(true);
  scroller_->layer()->SetFillsBoundsOpaquely(false);
  scroller_->layer()->SetMasksToBounds(true);

  empty_list_view_.reset(new NoNotificationMessageView);
  empty_list_view_->set_owned_by_client();
  message_list_view_.reset(new MessageListView(this, top_down));
  message_list_view_->set_owned_by_client();

  // We want to swap the contents of the scroll view between the empty list
  // view and the message list view, without constructing them afresh each
  // time.  So, since the scroll view deletes old contents each time you
  // set the contents (regardless of the |owned_by_client_| setting) we need
  // an intermediate view for the contents whose children we can swap in and
  // out.
  views::View* scroller_contents = new views::View();
  scroller_contents->SetLayoutManager(new views::FillLayout());
  scroller_contents->AddChildView(empty_list_view_.get());
  scroller_->SetContents(scroller_contents);

  settings_view_ = new NotifierSettingsView(notifier_settings_provider);

  if (initially_settings_visible)
    scroller_->SetVisible(false);
  else
    settings_view_->SetVisible(false);

  AddChildView(scroller_);
  AddChildView(settings_view_);
  AddChildView(button_bar_);
}

MessageCenterView::~MessageCenterView() {
  if (!is_closing_)
    message_center_->RemoveObserver(this);
}

void MessageCenterView::SetNotifications(
    const NotificationList::Notifications& notifications)  {
  if (is_closing_)
    return;

  notification_views_.clear();

  int index = 0;
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    AddNotificationAt(*(*iter), index++);

    message_center_->DisplayedNotification(
        (*iter)->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
    if (notification_views_.size() >= kMaxVisibleMessageCenterNotifications)
      break;
  }

  NotificationsChanged();
  scroller_->RequestFocus();
}

void MessageCenterView::SetSettingsVisible(bool visible) {
  if (is_closing_)
    return;

  if (visible == settings_visible_)
    return;

  settings_visible_ = visible;

  if (visible) {
    source_view_ = scroller_;
    target_view_ = settings_view_;
  } else {
    source_view_ = settings_view_;
    target_view_ = scroller_;
  }
  source_height_ = source_view_->GetHeightForWidth(width());
  target_height_ = target_view_->GetHeightForWidth(width());

  gfx::MultiAnimation::Parts parts;
  // First part: slide resize animation.
  parts.push_back(gfx::MultiAnimation::Part(
      (source_height_ == target_height_) ? 0 : kDefaultAnimationDurationMs,
      gfx::Tween::EASE_OUT));
  // Second part: fade-out the source_view.
  if (source_view_->layer()) {
    parts.push_back(gfx::MultiAnimation::Part(
        kDefaultAnimationDurationMs, gfx::Tween::LINEAR));
  } else {
    parts.push_back(gfx::MultiAnimation::Part());
  }
  // Third part: fade-in the target_view.
  if (target_view_->layer()) {
    parts.push_back(gfx::MultiAnimation::Part(
        kDefaultAnimationDurationMs, gfx::Tween::LINEAR));
    target_view_->layer()->SetOpacity(0);
    target_view_->SetVisible(true);
  } else {
    parts.push_back(gfx::MultiAnimation::Part());
  }
  settings_transition_animation_.reset(new gfx::MultiAnimation(
      parts, base::TimeDelta::FromMicroseconds(1000000 / kDefaultFrameRateHz)));
  settings_transition_animation_->set_delegate(this);
  settings_transition_animation_->set_continuous(false);
  settings_transition_animation_->Start();

  button_bar_->SetBackArrowVisible(visible);
}

void MessageCenterView::ClearAllClosableNotifications() {
  if (is_closing_)
    return;

  SetViewHierarchyEnabled(scroller_, false);
  button_bar_->SetAllButtonsEnabled(false);
  message_list_view_->ClearAllClosableNotifications(
      scroller_->GetVisibleRect());
}

void MessageCenterView::OnAllNotificationsCleared() {
  SetViewHierarchyEnabled(scroller_, true);
  button_bar_->SetAllButtonsEnabled(true);
  button_bar_->SetCloseAllButtonEnabled(false);

  // Action by user.
  message_center_->RemoveAllNotifications(
      true /* by_user */,
      message_center::MessageCenter::RemoveType::NON_PINNED);
}

size_t MessageCenterView::NumMessageViewsForTest() const {
  return message_list_view_->child_count();
}

void MessageCenterView::OnSettingsChanged() {
  scroller_->InvalidateLayout();
  PreferredSizeChanged();
  Layout();
}

void MessageCenterView::SetIsClosing(bool is_closing) {
  is_closing_ = is_closing;
  if (is_closing)
    message_center_->RemoveObserver(this);
  else
    message_center_->AddObserver(this);
}

void MessageCenterView::Layout() {
  if (is_closing_)
    return;

  int button_height = button_bar_->GetHeightForWidth(width()) +
                      button_bar_->GetInsets().height();
  // Skip unnecessary re-layout of contents during the resize animation.
  bool animating = settings_transition_animation_ &&
                   settings_transition_animation_->is_animating();
  if (animating && settings_transition_animation_->current_part_index() == 0) {
    if (!top_down_) {
      button_bar_->SetBounds(
          0, height() - button_height, width(), button_height);
    }
    return;
  }

  scroller_->SetBounds(0,
                       top_down_ ? button_height : 0,
                       width(),
                       height() - button_height);
  settings_view_->SetBounds(0,
                            top_down_ ? button_height : 0,
                            width(),
                            height() - button_height);

  bool is_scrollable = false;
  if (scroller_->visible())
    is_scrollable = scroller_->height() < message_list_view_->height();
  else
    is_scrollable = settings_view_->IsScrollable();

  if (!animating) {
    if (is_scrollable) {
      // Draw separator line on the top of the button bar if it is on the bottom
      // or draw it at the bottom if the bar is on the top.
      button_bar_->SetBorder(views::Border::CreateSolidSidedBorder(
          top_down_ ? 0 : 1, 0, top_down_ ? 1 : 0, 0, kFooterDelimiterColor));
    } else {
      button_bar_->SetBorder(views::Border::CreateEmptyBorder(
          top_down_ ? 0 : 1, 0, top_down_ ? 1 : 0, 0));
    }
    button_bar_->SchedulePaint();
  }
  button_bar_->SetBounds(0,
                         top_down_ ? 0 : height() - button_height,
                         width(),
                         button_height);
  if (GetWidget())
    GetWidget()->GetRootView()->SchedulePaint();
}

gfx::Size MessageCenterView::GetPreferredSize() const {
  if (settings_transition_animation_ &&
      settings_transition_animation_->is_animating()) {
    int content_width = std::max(source_view_->GetPreferredSize().width(),
                                 target_view_->GetPreferredSize().width());
    int width = std::max(content_width,
                         button_bar_->GetPreferredSize().width());
    return gfx::Size(width, GetHeightForWidth(width));
  }

  int width = 0;
  for (int i = 0; i < child_count(); ++i) {
    const views::View* child = child_at(0);
    if (child->visible())
      width = std::max(width, child->GetPreferredSize().width());
  }
  return gfx::Size(width, GetHeightForWidth(width));
}

int MessageCenterView::GetHeightForWidth(int width) const {
  if (settings_transition_animation_ &&
      settings_transition_animation_->is_animating()) {
    int content_height = target_height_;
    if (settings_transition_animation_->current_part_index() == 0) {
      content_height = settings_transition_animation_->CurrentValueBetween(
          source_height_, target_height_);
    }
    return button_bar_->GetHeightForWidth(width) + content_height;
  }

  int content_height = 0;
  if (scroller_->visible())
    content_height += scroller_->GetHeightForWidth(width);
  else
    content_height += settings_view_->GetHeightForWidth(width);
  return button_bar_->GetHeightForWidth(width) +
         button_bar_->GetInsets().height() + content_height;
}

bool MessageCenterView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // Do not rely on the default scroll event handler of ScrollView because
  // the scroll happens only when the focus is on the ScrollView. The
  // notification center will allow the scrolling even when the focus is on
  // the buttons.
  if (scroller_->bounds().Contains(event.location()))
    return scroller_->OnMouseWheel(event);
  return views::View::OnMouseWheel(event);
}

void MessageCenterView::OnMouseExited(const ui::MouseEvent& event) {
  if (is_closing_)
    return;

  message_list_view_->ResetRepositionSession();
  NotificationsChanged();
}

void MessageCenterView::OnNotificationAdded(const std::string& id) {
  int index = 0;
  const NotificationList::Notifications& notifications =
      message_center_->GetVisibleNotifications();
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end();
       ++iter, ++index) {
    if ((*iter)->id() == id) {
      AddNotificationAt(*(*iter), index);
      break;
    }
    if (notification_views_.size() >= kMaxVisibleMessageCenterNotifications)
      break;
  }
  NotificationsChanged();
}

void MessageCenterView::OnNotificationRemoved(const std::string& id,
                                              bool by_user) {
  NotificationViewsMap::iterator view_iter = notification_views_.find(id);
  if (view_iter == notification_views_.end())
    return;
  NotificationView* view = view_iter->second;
  int index = message_list_view_->GetIndexOf(view);
  DCHECK_LE(0, index);
  if (by_user) {
    message_list_view_->SetRepositionTarget(view->bounds());
    // Moves the keyboard focus to the next notification if the removed
    // notification is focused so that the user can dismiss notifications
    // without re-focusing by tab key.
    if (view->IsCloseButtonFocused() ||
        view == GetFocusManager()->GetFocusedView()) {
      views::View* next_focused_view = NULL;
      if (message_list_view_->child_count() > index + 1)
        next_focused_view = message_list_view_->child_at(index + 1);
      else if (index > 0)
        next_focused_view = message_list_view_->child_at(index - 1);

      if (next_focused_view) {
        if (view->IsCloseButtonFocused()) {
          // Safe cast since all views in MessageListView are MessageViews.
          static_cast<MessageView*>(
              next_focused_view)->RequestFocusOnCloseButton();
        } else {
          next_focused_view->RequestFocus();
        }
      }
    }
  }
  message_list_view_->RemoveNotification(view);
  notification_views_.erase(view_iter);
  NotificationsChanged();
}

void MessageCenterView::OnNotificationUpdated(const std::string& id) {
  NotificationViewsMap::const_iterator view_iter = notification_views_.find(id);
  if (view_iter == notification_views_.end())
    return;

  // Set the item on the mouse cursor as the reposition target so that it
  // should stick to the current position over the update.
  bool set = false;
  if (message_list_view_->IsMouseHovered()) {
    for (const auto& hover_id_view : notification_views_) {
      NotificationView* hover_view = hover_id_view.second;
      if (hover_view->IsMouseHovered()) {
        message_list_view_->SetRepositionTarget(hover_view->bounds());
        set = true;
        break;
      }
    }
  }
  if (!set)
    message_list_view_->ResetRepositionSession();

  // TODO(dimich): add MessageCenter::GetVisibleNotificationById(id)
  NotificationView* view = view_iter->second;
  const NotificationList::Notifications& notifications =
      message_center_->GetVisibleNotifications();
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    if ((*iter)->id() == id) {
      int old_width = view->width();
      int old_height = view->GetHeightForWidth(old_width);
      message_list_view_->UpdateNotification(view, **iter);
      if (view->GetHeightForWidth(old_width) != old_height)
        NotificationsChanged();
      break;
    }
  }
}

void MessageCenterView::ClickOnNotification(
    const std::string& notification_id) {
  message_center_->ClickOnNotification(notification_id);
}

void MessageCenterView::RemoveNotification(const std::string& notification_id,
                                           bool by_user) {
  message_center_->RemoveNotification(notification_id, by_user);
}

std::unique_ptr<ui::MenuModel> MessageCenterView::CreateMenuModel(
    const NotifierId& notifier_id,
    const base::string16& display_source) {
  return tray_->CreateNotificationMenuModel(notifier_id, display_source);
}

bool MessageCenterView::HasClickedListener(const std::string& notification_id) {
  return message_center_->HasClickedListener(notification_id);
}

void MessageCenterView::ClickOnNotificationButton(
    const std::string& notification_id,
    int button_index) {
  message_center_->ClickOnNotificationButton(notification_id, button_index);
}

void MessageCenterView::ClickOnSettingsButton(
    const std::string& notification_id) {
  message_center_->ClickOnSettingsButton(notification_id);
}

void MessageCenterView::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, settings_transition_animation_.get());

  Visibility visibility = target_view_ == settings_view_
                              ? VISIBILITY_SETTINGS
                              : VISIBILITY_MESSAGE_CENTER;
  message_center_->SetVisibility(visibility);

  source_view_->SetVisible(false);
  target_view_->SetVisible(true);
  if (source_view_->layer())
    source_view_->layer()->SetOpacity(1.0);
  if (target_view_->layer())
    target_view_->layer()->SetOpacity(1.0);
  settings_transition_animation_.reset();
  PreferredSizeChanged();
  Layout();
}

void MessageCenterView::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(animation, settings_transition_animation_.get());
  PreferredSizeChanged();
  if (settings_transition_animation_->current_part_index() == 1 &&
      source_view_->layer()) {
    source_view_->layer()->SetOpacity(
        1.0 - settings_transition_animation_->GetCurrentValue());
    SchedulePaint();
  } else if (settings_transition_animation_->current_part_index() == 2 &&
             target_view_->layer()) {
    target_view_->layer()->SetOpacity(
        settings_transition_animation_->GetCurrentValue());
    SchedulePaint();
  }
}

void MessageCenterView::AnimationCanceled(const gfx::Animation* animation) {
  DCHECK_EQ(animation, settings_transition_animation_.get());
  AnimationEnded(animation);
}

void MessageCenterView::AddNotificationAt(const Notification& notification,
                                          int index) {
  NotificationView* view =
      NotificationView::Create(this, notification, false);  // Not top-level.
  view->set_context_menu_controller(context_menu_controller_.get());
  notification_views_[notification.id()] = view;
  view->set_scroller(scroller_);
  message_list_view_->AddNotificationAt(view, index);
}

void MessageCenterView::NotificationsChanged() {
  bool no_message_views = notification_views_.empty();

  // When the child view is removed from the hierarchy, its focus is cleared.
  // In this case we want to save which view has focus so that the user can
  // continue to interact with notifications in the order they were expecting.
  views::FocusManager* focus_manager = scroller_->GetFocusManager();
  View* focused_view = NULL;
  // |focus_manager| can be NULL in tests.
  if (focus_manager)
    focused_view = focus_manager->GetFocusedView();

  // All the children of this view are owned by |this|.
  scroller_->contents()->RemoveAllChildViews(/*delete_children=*/false);
  scroller_->contents()->AddChildView(
      no_message_views ? empty_list_view_.get() : message_list_view_.get());

  bool no_closable_views = true;
  for (const auto& view : notification_views_) {
    if (!view.second->IsPinned()) {
      no_closable_views = false;
      break;
    }
  }
  button_bar_->SetCloseAllButtonEnabled(!no_closable_views);
  scroller_->SetFocusBehavior(no_message_views ? FocusBehavior::NEVER
                                               : FocusBehavior::ALWAYS);

  if (focus_manager && focused_view)
    focus_manager->SetFocusedView(focused_view);

  scroller_->InvalidateLayout();
  PreferredSizeChanged();
  Layout();
}

void MessageCenterView::SetNotificationViewForTest(MessageView* view) {
  message_list_view_->AddNotificationAt(view, 0);
}

}  // namespace message_center
