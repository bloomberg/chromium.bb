// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_center_view.h"

#include <list>
#include <map>

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/views/message_center_button_bar.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/message_center/views/notifier_settings_view.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace message_center {

namespace {

const SkColor kBorderDarkColor = SkColorSetRGB(0xaa, 0xaa, 0xaa);
const SkColor kButtonTextHighlightColor = SkColorSetRGB(0x2a, 0x2a, 0x2a);
const SkColor kButtonTextHoverColor = SkColorSetRGB(0x2a, 0x2a, 0x2a);
const SkColor kNoNotificationsTextColor = SkColorSetRGB(0xb4, 0xb4, 0xb4);
const SkColor kTransparentColor = SkColorSetARGB(0, 0, 0, 0);
const int kAnimateClearingNextNotificationDelayMS = 40;
const int kButtonBarBorderThickness = 1;
const int kMinScrollViewHeight = 100;

static const int kDefaultAnimationDurationMs = 120;
static const int kDefaultFrameRateHz = 60;

}  // namespace

// BoundedScrollView ///////////////////////////////////////////////////////////

// A custom scroll view whose height has a minimum and maximum value and whose
// scroll bar disappears when not needed.
class BoundedScrollView : public views::ScrollView {
 public:
  BoundedScrollView(int min_height, int max_height);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void Layout() OVERRIDE;

 private:
  int min_height_;
  int max_height_;

  DISALLOW_COPY_AND_ASSIGN(BoundedScrollView);
};

BoundedScrollView::BoundedScrollView(int min_height, int max_height)
    : min_height_(min_height),
      max_height_(max_height) {
  set_notify_enter_exit_on_child(true);
  // Cancels the default dashed focus border.
  set_focus_border(NULL);
  set_background(
      views::Background::CreateSolidBackground(kMessageCenterBackgroundColor));
  SetVerticalScrollBar(new views::OverlayScrollBar(false));
}

gfx::Size BoundedScrollView::GetPreferredSize() {
  gfx::Size size = contents()->GetPreferredSize();
  size.SetToMax(gfx::Size(size.width(), min_height_));
  size.SetToMin(gfx::Size(size.width(), max_height_));
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

int BoundedScrollView::GetHeightForWidth(int width) {
  gfx::Insets insets = GetInsets();
  width = std::max(0, width - insets.width());
  int height = contents()->GetHeightForWidth(width) + insets.height();
  return std::min(std::max(height, min_height_), max_height_);
}

void BoundedScrollView::Layout() {
  int content_width = width();
  int content_height = contents()->GetHeightForWidth(content_width);
  if (content_height > height()) {
    content_width = std::max(content_width - GetScrollBarWidth(), 0);
    content_height = contents()->GetHeightForWidth(content_width);
  }
  if (contents()->bounds().size() != gfx::Size(content_width, content_height))
    contents()->SetBounds(0, 0, content_width, content_height);
  views::ScrollView::Layout();
}

class NoNotificationMessageView : public views::View {
 public:
  NoNotificationMessageView();
  virtual ~NoNotificationMessageView();

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void Layout() OVERRIDE;

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

gfx::Size NoNotificationMessageView::GetPreferredSize() {
  return gfx::Size(kMinScrollViewHeight, label_->GetPreferredSize().width());
}

int NoNotificationMessageView::GetHeightForWidth(int width) {
  return kMinScrollViewHeight;
}

void NoNotificationMessageView::Layout() {
  int text_height = label_->GetHeightForWidth(width());
  int margin = (height() - text_height) / 2;
  label_->SetBounds(0, margin, width(), text_height);
}

// Displays a list of messages for rich notifications. It also supports
// repositioning.
class MessageListView : public views::View,
                        public views::BoundsAnimatorObserver {
 public:
  explicit MessageListView(MessageCenterView* message_center_view,
                           bool top_down);
  virtual ~MessageListView();

  void AddNotificationAt(views::View* view, int i);
  void RemoveNotificationAt(int i);
  void UpdateNotificationAt(views::View* view, int i);
  void SetRepositionTarget(const gfx::Rect& target_rect);
  void ResetRepositionSession();
  void ClearAllNotifications(const gfx::Rect& visible_scroll_rect);

 protected:
  // Overridden from views::View.
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;
  virtual void ReorderChildLayers(ui::Layer* parent_layer) OVERRIDE;

  // Overridden from views::BoundsAnimatorObserver.
  virtual void OnBoundsAnimatorProgressed(
      views::BoundsAnimator* animator) OVERRIDE;
  virtual void OnBoundsAnimatorDone(views::BoundsAnimator* animator) OVERRIDE;

 private:
  // Returns the actual index for child of |index|.
  // MessageListView allows to slide down upper notifications, which means
  // that the upper ones should come above the lower ones if top_down is not
  // enabled. To achieve this, inversed order is adopted. The top most
  // notification is the last child, and the bottom most notification is the
  // first child.
  int GetActualIndex(int index);
  bool IsValidChild(views::View* child);
  void DoUpdateIfPossible();

  // Animates all notifications below target upwards to align with the top of
  // the last closed notification.
  void AnimateNotificationsBelowTarget();
  // Animates all notifications above target downwards to align with the top of
  // the last closed notification.
  void AnimateNotificationsAboveTarget();

  // Schedules animation for a child to the specified position. Returns false
  // if |child| will disappear after the animation.
  bool AnimateChild(views::View* child, int top, int height);

  // Animate clearing one notification.
  void AnimateClearingOneNotification();
  MessageCenterView* message_center_view() const {
    return message_center_view_;
  }

  MessageCenterView* message_center_view_;  // Weak reference.
  // The top position of the reposition target rectangle.
  int reposition_top_;
  int fixed_height_;
  bool has_deferred_task_;
  bool clear_all_started_;
  bool top_down_;
  std::set<views::View*> adding_views_;
  std::set<views::View*> deleting_views_;
  std::set<views::View*> deleted_when_done_;
  std::list<views::View*> clearing_all_views_;
  scoped_ptr<views::BoundsAnimator> animator_;
  base::WeakPtrFactory<MessageListView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MessageListView);
};

MessageListView::MessageListView(MessageCenterView* message_center_view,
                                 bool top_down)
    : message_center_view_(message_center_view),
      reposition_top_(-1),
      fixed_height_(0),
      has_deferred_task_(false),
      clear_all_started_(false),
      top_down_(top_down),
      weak_ptr_factory_(this) {
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1);
  layout->set_spread_blank_space(true);
  SetLayoutManager(layout);

  // Set the margin to 0 for the layout. BoxLayout assumes the same margin
  // for top and bottom, but the bottom margin here should be smaller
  // because of the shadow of message view. Use an empty border instead
  // to provide this margin.
  gfx::Insets shadow_insets = MessageView::GetShadowInsets();
  set_background(views::Background::CreateSolidBackground(
      kMessageCenterBackgroundColor));
  set_border(views::Border::CreateEmptyBorder(
      kMarginBetweenItems - shadow_insets.top(), /* top */
      kMarginBetweenItems - shadow_insets.left(), /* left */
      kMarginBetweenItems - shadow_insets.bottom(),  /* bottom */
      kMarginBetweenItems - shadow_insets.right() /* right */ ));
}

MessageListView::~MessageListView() {
  if (animator_.get())
    animator_->RemoveObserver(this);
}

void MessageListView::Layout() {
  if (animator_.get())
    return;

  gfx::Rect child_area = GetContentsBounds();
  int top = child_area.y();
  int between_items =
      kMarginBetweenItems - MessageView::GetShadowInsets().bottom();

  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;
    int height = child->GetHeightForWidth(child_area.width());
    child->SetBounds(child_area.x(), top, child_area.width(), height);
    top += height + between_items;
  }
}

void MessageListView::AddNotificationAt(views::View* view, int i) {
  AddChildViewAt(view, GetActualIndex(i));
  if (GetContentsBounds().IsEmpty())
    return;

  adding_views_.insert(view);
  DoUpdateIfPossible();
}

void MessageListView::RemoveNotificationAt(int i) {
  views::View* child = child_at(GetActualIndex(i));
  if (GetContentsBounds().IsEmpty()) {
    delete child;
  } else {
    if (child->layer()) {
      deleting_views_.insert(child);
    } else {
      if (animator_.get())
        animator_->StopAnimatingView(child);
      delete child;
    }
    DoUpdateIfPossible();
  }
}

void MessageListView::UpdateNotificationAt(views::View* view, int i) {
  int actual_index = GetActualIndex(i);
  views::View* child = child_at(actual_index);
  if (animator_.get())
    animator_->StopAnimatingView(child);
  gfx::Rect old_bounds = child->bounds();
  if (deleting_views_.find(child) != deleting_views_.end())
    deleting_views_.erase(child);
  if (deleted_when_done_.find(child) != deleted_when_done_.end())
    deleted_when_done_.erase(child);
  delete child;
  AddChildViewAt(view, actual_index);
  view->SetBounds(old_bounds.x(), old_bounds.y(), old_bounds.width(),
                  view->GetHeightForWidth(old_bounds.width()));
  DoUpdateIfPossible();
}

gfx::Size MessageListView::GetPreferredSize() {
  int width = 0;
  for (int i = 0; i < child_count(); i++) {
    views::View* child = child_at(i);
    if (IsValidChild(child))
      width = std::max(width, child->GetPreferredSize().width());
  }

  return gfx::Size(width + GetInsets().width(),
                   GetHeightForWidth(width + GetInsets().width()));
}

int MessageListView::GetHeightForWidth(int width) {
  if (fixed_height_ > 0)
    return fixed_height_;

  width -= GetInsets().width();
  int height = 0;
  int padding = 0;
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!IsValidChild(child))
      continue;
    height += child->GetHeightForWidth(width) + padding;
    padding = kMarginBetweenItems - MessageView::GetShadowInsets().bottom();
  }

  return height + GetInsets().height();
}

void MessageListView::PaintChildren(gfx::Canvas* canvas) {
  // Paint in the inversed order. Otherwise upper notification may be
  // hidden by the lower one.
  for (int i = child_count() - 1; i >= 0; --i) {
    if (!child_at(i)->layer())
      child_at(i)->Paint(canvas);
  }
}

void MessageListView::ReorderChildLayers(ui::Layer* parent_layer) {
  // Reorder children to stack the last child layer at the top. Otherwise
  // upper notification may be hidden by the lower one.
  for (int i = 0; i < child_count(); ++i) {
    if (child_at(i)->layer())
      parent_layer->StackAtBottom(child_at(i)->layer());
  }
}

void MessageListView::SetRepositionTarget(const gfx::Rect& target) {
  reposition_top_ = target.y();
  fixed_height_ = GetHeightForWidth(width());
}

void MessageListView::ResetRepositionSession() {
  // Don't call DoUpdateIfPossible(), but let Layout() do the task without
  // animation. Reset will cause the change of the bubble size itself, and
  // animation from the old location will look weird.
  if (reposition_top_ >= 0 && animator_.get()) {
    has_deferred_task_ = false;
    // cancel cause OnBoundsAnimatorDone which deletes |deleted_when_done_|.
    animator_->Cancel();
    STLDeleteContainerPointers(deleting_views_.begin(), deleting_views_.end());
    deleting_views_.clear();
    adding_views_.clear();
    animator_.reset();
  }

  reposition_top_ = -1;
  fixed_height_ = 0;
}

void MessageListView::ClearAllNotifications(
    const gfx::Rect& visible_scroll_rect) {
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;
    if (gfx::IntersectRects(child->bounds(), visible_scroll_rect).IsEmpty())
      continue;
    clearing_all_views_.push_back(child);
  }
  DoUpdateIfPossible();
}

void MessageListView::OnBoundsAnimatorProgressed(
    views::BoundsAnimator* animator) {
  DCHECK_EQ(animator_.get(), animator);
  for (std::set<views::View*>::iterator iter = deleted_when_done_.begin();
       iter != deleted_when_done_.end(); ++iter) {
    const gfx::SlideAnimation* animation = animator->GetAnimationForView(*iter);
    if (animation)
      (*iter)->layer()->SetOpacity(animation->CurrentValueBetween(1.0, 0.0));
  }
}

void MessageListView::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  STLDeleteContainerPointers(
      deleted_when_done_.begin(), deleted_when_done_.end());
  deleted_when_done_.clear();

  if (clear_all_started_) {
    clear_all_started_ = false;
    message_center_view()->OnAllNotificationsCleared();
  }

  if (has_deferred_task_) {
    has_deferred_task_ = false;
    DoUpdateIfPossible();
  }

  if (GetWidget())
    GetWidget()->SynthesizeMouseMoveEvent();
}

int MessageListView::GetActualIndex(int index) {
  for (int i = 0; i < child_count() && i <= index; ++i)
    index += IsValidChild(child_at(i)) ? 0 : 1;
  return std::min(index, child_count());
}

bool MessageListView::IsValidChild(views::View* child) {
  return child->visible() &&
         deleting_views_.find(child) == deleting_views_.end() &&
         deleted_when_done_.find(child) == deleted_when_done_.end();
}

void MessageListView::DoUpdateIfPossible() {
  gfx::Rect child_area = GetContentsBounds();
  if (child_area.IsEmpty())
    return;

  if (animator_.get() && animator_->IsAnimating()) {
    has_deferred_task_ = true;
    return;
  }

  if (!animator_.get()) {
    animator_.reset(new views::BoundsAnimator(this));
    animator_->AddObserver(this);
  }

  if (!clearing_all_views_.empty()) {
    AnimateClearingOneNotification();
    return;
  }

  if (top_down_)
    AnimateNotificationsBelowTarget();
  else
    AnimateNotificationsAboveTarget();

  adding_views_.clear();
  deleting_views_.clear();
}

void MessageListView::AnimateNotificationsBelowTarget() {
  int last_index = -1;
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!IsValidChild(child)) {
      AnimateChild(child, child->y(), child->height());
    } else if (reposition_top_ < 0 || child->y() > reposition_top_) {
      // Find first notification below target (or all notifications if no
      // target).
      last_index = i;
      break;
    }
  }
  if (last_index > 0) {
    int between_items =
        kMarginBetweenItems - MessageView::GetShadowInsets().bottom();
    int top = (reposition_top_ > 0) ? reposition_top_ : GetInsets().top();

    for (int i = last_index; i < child_count(); ++i) {
      // Animate notifications below target upwards.
      views::View* child = child_at(i);
      if (AnimateChild(child, top, child->height()))
        top += child->height() + between_items;
    }
  }
}

void MessageListView::AnimateNotificationsAboveTarget() {
  int last_index = -1;
  for (int i = child_count() - 1; i >= 0; --i) {
    views::View* child = child_at(i);
    if (!IsValidChild(child)) {
      AnimateChild(child, child->y(), child->height());
    } else if (reposition_top_ < 0 || child->y() < reposition_top_) {
      // Find first notification above target (or all notifications if no
      // target).
      last_index = i;
      break;
    }
  }
  if (last_index > 0) {
    int between_items =
        kMarginBetweenItems - MessageView::GetShadowInsets().bottom();
    int bottom = (reposition_top_ > 0)
                     ? reposition_top_ + child_at(last_index)->height()
                     : GetHeightForWidth(width()) - GetInsets().bottom();
    for (int i = last_index; i >= 0; --i) {
      // Animate notifications above target downwards.
      views::View* child = child_at(i);
      if (AnimateChild(child, bottom - child->height(), child->height()))
        bottom -= child->height() + between_items;
    }
  }
}

bool MessageListView::AnimateChild(views::View* child, int top, int height) {
  gfx::Rect child_area = GetContentsBounds();
  if (adding_views_.find(child) != adding_views_.end()) {
    child->SetBounds(child_area.right(), top, child_area.width(), height);
    animator_->AnimateViewTo(
        child, gfx::Rect(child_area.x(), top, child_area.width(), height));
  } else if (deleting_views_.find(child) != deleting_views_.end()) {
    DCHECK(child->layer());
    // No moves, but animate to fade-out.
    animator_->AnimateViewTo(child, child->bounds());
    deleted_when_done_.insert(child);
    return false;
  } else {
    gfx::Rect target(child_area.x(), top, child_area.width(), height);
    if (child->bounds().origin() != target.origin())
      animator_->AnimateViewTo(child, target);
    else
      child->SetBoundsRect(target);
  }
  return true;
}

void MessageListView::AnimateClearingOneNotification() {
  DCHECK(!clearing_all_views_.empty());

  clear_all_started_ = true;

  views::View* child = clearing_all_views_.front();
  clearing_all_views_.pop_front();

  // Slide from left to right.
  gfx::Rect new_bounds = child->bounds();
  new_bounds.set_x(new_bounds.right() + kMarginBetweenItems);
  animator_->AnimateViewTo(child, new_bounds);

  // Schedule to start sliding out next notification after a short delay.
  if (!clearing_all_views_.empty()) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MessageListView::AnimateClearingOneNotification,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            kAnimateClearingNextNotificationDelayMS));
  }
}

// MessageCenterView ///////////////////////////////////////////////////////////

MessageCenterView::MessageCenterView(MessageCenter* message_center,
                                     MessageCenterTray* tray,
                                     int max_height,
                                     bool initially_settings_visible,
                                     bool top_down)
    : message_center_(message_center),
      tray_(tray),
      top_down_(top_down),
      settings_visible_(initially_settings_visible),
      source_view_(NULL),
      source_height_(0),
      target_view_(NULL),
      target_height_(0),
      is_closing_(false) {
  message_center_->AddObserver(this);
  set_notify_enter_exit_on_child(true);
  set_background(views::Background::CreateSolidBackground(
      kMessageCenterBackgroundColor));

  NotifierSettingsProvider* notifier_settings_provider =
      message_center_->GetNotifierSettingsProvider();
  button_bar_ = new MessageCenterButtonBar(this,
                                           message_center,
                                           notifier_settings_provider,
                                           initially_settings_visible);

  const int button_height = button_bar_->GetPreferredSize().height();

  scroller_ =
      new BoundedScrollView(kMinScrollViewHeight, max_height - button_height);

  if (get_use_acceleration_when_possible()) {
    scroller_->SetPaintToLayer(true);
    scroller_->SetFillsBoundsOpaquely(false);
    scroller_->layer()->SetMasksToBounds(true);
  }

  message_list_view_ = new MessageListView(this, top_down);
  no_notifications_message_view_ = new NoNotificationMessageView();
  // Set the default visibility to false, otherwise the notification has slide
  // in animation when the center is shown.
  no_notifications_message_view_->SetVisible(false);
  message_list_view_->AddChildView(no_notifications_message_view_);
  scroller_->SetContents(message_list_view_);

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

  message_views_.clear();
  int index = 0;
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end();
       ++iter, ++index) {
    AddNotificationAt(*(*iter), index);
    if (message_views_.size() >= kMaxVisibleMessageCenterNotifications)
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

void MessageCenterView::ClearAllNotifications() {
  if (is_closing_)
    return;

  scroller_->SetEnabled(false);
  button_bar_->SetAllButtonsEnabled(false);
  message_list_view_->ClearAllNotifications(scroller_->GetVisibleRect());
}

void MessageCenterView::OnAllNotificationsCleared() {
  scroller_->SetEnabled(true);
  button_bar_->SetAllButtonsEnabled(true);
  button_bar_->SetCloseAllButtonEnabled(false);
  message_center_->RemoveAllNotifications(true);  // Action by user.
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
  if (settings_transition_animation_ &&
      settings_transition_animation_->is_animating() &&
      settings_transition_animation_->current_part_index() == 0) {
    if (!top_down_)
      button_bar_->SetBounds(
          0, height() - button_height, width(), button_height);
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

  if (is_scrollable && !button_bar_->border()) {
    // Draw separator line on the top of the button bar if it is on the bottom
    // or draw it at the bottom if the bar is on the top.
    button_bar_->set_border(views::Border::CreateSolidSidedBorder(
        top_down_ ? 0 : 1,
        0,
        top_down_ ? 1 : 0,
        0,
        kFooterDelimiterColor));
    button_bar_->SchedulePaint();
  } else if (!is_scrollable && button_bar_->border()) {
    button_bar_->set_border(NULL);
    button_bar_->SchedulePaint();
  }
  button_bar_->SetBounds(0,
                         top_down_ ? 0 : height() - button_height,
                         width(),
                         button_height);
  if (GetWidget())
    GetWidget()->GetRootView()->SchedulePaint();
}

gfx::Size MessageCenterView::GetPreferredSize() {
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
    views::View* child = child_at(0);
    if (child->visible())
      width = std::max(width, child->GetPreferredSize().width());
  }
  return gfx::Size(width, GetHeightForWidth(width));
}

int MessageCenterView::GetHeightForWidth(int width) {
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
    if (message_views_.size() >= kMaxVisibleMessageCenterNotifications)
      break;
  }
  NotificationsChanged();
}

void MessageCenterView::OnNotificationRemoved(const std::string& id,
                                              bool by_user) {
  for (size_t i = 0; i < message_views_.size(); ++i) {
    if (message_views_[i]->notification_id() == id) {
      if (by_user) {
        message_list_view_->SetRepositionTarget(message_views_[i]->bounds());
        // Moves the keyboard focus to the next notification if the removed
        // notification is focused so that the user can dismiss notifications
        // without re-focusing by tab key.
        if (message_views_.size() > 1) {
          views::View* focused_view = GetFocusManager()->GetFocusedView();
          if (message_views_[i]->IsCloseButtonFocused() ||
              focused_view == message_views_[i]) {
            size_t next_index = i + 1;
            if (next_index >= message_views_.size())
              next_index = message_views_.size() - 2;
            if (focused_view == message_views_[i])
              message_views_[next_index]->RequestFocus();
            else
              message_views_[next_index]->RequestFocusOnCloseButton();
          }
        }
      }
      message_list_view_->RemoveNotificationAt(i);
      message_views_.erase(message_views_.begin() + i);
      NotificationsChanged();
      break;
    }
  }
}

void MessageCenterView::OnNotificationUpdated(const std::string& id) {
  const NotificationList::Notifications& notifications =
      message_center_->GetVisibleNotifications();
  size_t index = 0;
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin();
       iter != notifications.end() && index < message_views_.size();
       ++iter, ++index) {
    DCHECK((*iter)->id() == message_views_[index]->notification_id());
    if ((*iter)->id() == id) {
      MessageView* view =
          NotificationView::Create(*(*iter),
                                   message_center_,
                                   tray_,
                                   true,   // Create expanded.
                                   false); // Not creating a top-level
                                           // notification.
      view->set_scroller(scroller_);
      message_list_view_->UpdateNotificationAt(view, index);
      message_views_[index] = view;
      NotificationsChanged();
      break;
    }
  }
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
  // NotificationViews are expanded by default here until
  // http://crbug.com/217902 is fixed. TODO(dharcourt): Fix.
  MessageView* view =
      NotificationView::Create(notification,
                               message_center_,
                               tray_,
                               true,    // Create expanded.
                               false);  // Not creating a top-level
                                        // notification.
  view->set_scroller(scroller_);
  message_views_.insert(message_views_.begin() + index, view);
  message_list_view_->AddNotificationAt(view, index);
  message_center_->DisplayedNotification(notification.id());
}

void MessageCenterView::NotificationsChanged() {
  bool no_message_views = message_views_.empty();

  no_notifications_message_view_->SetVisible(no_message_views);
  button_bar_->SetCloseAllButtonEnabled(!no_message_views);
  scroller_->set_focusable(!no_message_views);

  scroller_->InvalidateLayout();
  PreferredSizeChanged();
  Layout();
}

void MessageCenterView::SetNotificationViewForTest(views::View* view) {
  message_list_view_->AddNotificationAt(view, 0);
}

}  // namespace message_center
