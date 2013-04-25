// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_popup_collection.h"

#include <set>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/time.h"
#include "base/timer.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_constants.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace message_center {
namespace {

// Timeout between the last user-initiated close of the toast and the moment
// when normal layout/update of the toast stack continues. If the last toast was
// just closed, the timeout is shorter.
const int kUpdateTimeoutOnUserActionMs = 2000;
const int kUpdateTimeoutOnUserActionEmptyMs = 700;

const int kToastMargin = kMarginBetweenItems;

// The width of a toast before animated revealed and after closing.
const int kClosedToastWidth = 5;

gfx::Size GetToastSize(views::View* view) {
  int width = kNotificationWidth + view->GetInsets().width();
  return gfx::Size(width, view->GetHeightForWidth(width));
}

}  // namespace.

class ToastContentsView : public views::WidgetDelegateView,
                          public ui::AnimationDelegate {
 public:
  ToastContentsView(const Notification* notification,
                    base::WeakPtr<MessagePopupCollection> collection,
                    MessageCenter* message_center)
      : id_(notification->id()),
        collection_(collection),
        message_center_(message_center),
        is_animating_bounds_(false),
        is_closing_(false),
        closing_animation_(NULL) {
    DCHECK(collection_);

    set_notify_enter_exit_on_child(true);
    // Sets the transparent background. Then, when the message view is slid out,
    // the whole toast seems to slide although the actual bound of the widget
    // remains. This is hacky but easier to keep the consistency.
    set_background(views::Background::CreateSolidBackground(0, 0, 0, 0));

    // Creates the timer only when it does the timeout (i.e. not never-timeout).
    if (!notification->never_timeout()) {
      timer_.reset(new base::OneShotTimer<ToastContentsView>);
      ResetTimeout(notification->priority());
      StartTimer();
    }
  }

  // This is destroyed when the toast window closes.
  virtual ~ToastContentsView() {
  }

  views::Widget* CreateWidget(gfx::NativeView parent) {
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_POPUP);
    params.keep_on_top = true;
    if (parent)
      params.parent = parent;
    else
      params.top_level = true;
    params.transparent = true;
    params.delegate = this;
    views::Widget* widget = new views::Widget();
    widget->set_focus_on_creation(false);
    widget->Init(params);
    return widget;
  }

  void SetContents(MessageView* view) {
    RemoveAllChildViews(true);
    AddChildView(view);
    preferred_size_ = GetToastSize(view);
    Layout();
  }

  void ResetTimeout(int priority) {
    int seconds = kAutocloseDefaultDelaySeconds;
    if (priority > DEFAULT_PRIORITY)
      seconds = kAutocloseHighPriorityDelaySeconds;
    timeout_ = base::TimeDelta::FromSeconds(seconds);
    // If timer exists and is not suspended, re-start it with new timeout.
    if (timer_.get() && timer_->IsRunning())
      StartTimer();
  }

  void SuspendTimer() {
    if (!timer_.get())
      return;
    timer_->Stop();
    passed_ += base::Time::Now() - start_time_;
  }

  void StartTimer() {
    if (!timer_.get())
      return;

    if (timeout_ <= passed_) {
      CloseWithAnimation();
    } else {
      start_time_ = base::Time::Now();
      timer_->Start(FROM_HERE,
                    timeout_ - passed_,
                    base::Bind(&ToastContentsView::CloseWithAnimation,
                               base::Unretained(this)));
    }
  }

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }

  virtual void WindowClosing() OVERRIDE {
    SuspendTimer();
  }

  virtual bool CanActivate() const OVERRIDE {
#if defined(OS_WIN) && defined(USE_AURA)
    return true;
#else
    return false;
#endif
  }

  // Overridden from views::View:
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE {
    if (collection_)
      collection_->OnMouseEntered();
  }

  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE {
    if (collection_)
      collection_->OnMouseExited();
  }

  virtual void Layout() OVERRIDE {
    if (child_count() > 0) {
      child_at(0)->SetBounds(
          0, 0, preferred_size_.width(), preferred_size_.height());
    }
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return child_count() ? GetToastSize(child_at(0)) : gfx::Size();
  }

  // Given the bounds of a toast on the screen, compute the bouds for that
  // toast in 'closed' state. The 'closed' state is used as origin/destination
  // in reveal/closing animations.
  gfx::Rect GetClosedToastBounds(gfx::Rect bounds) {
    return gfx::Rect(bounds.x() + bounds.width() - kClosedToastWidth,
                     bounds.y(),
                     kClosedToastWidth,
                     bounds.height());
  }

  // Shows the new toast for the first time, animated.
  // |origin| is the right-bottom corner if the toast.
  void RevealWithAnimation(gfx::Point origin) {
    // Place/move the toast widgets. Currently it stacks the widgets from the
    // right-bottom of the work area.
    // TODO(mukai): allow to specify the placement policy from outside of this
    // class. The policy should be specified from preference on Windows, or
    // the launcher alignment on ChromeOS.
    origin_ = gfx::Point(origin.x() - preferred_size_.width(),
                         origin.y() - preferred_size_.height());

    gfx::Rect stable_bounds(origin_, preferred_size_);

    SetBoundsInstantly(GetClosedToastBounds(stable_bounds));
    GetWidget()->Show();
    SetBoundsWithAnimation(stable_bounds);
  }

  void CloseWithAnimation() {
    if (is_closing_)
      return;
    is_closing_ = true;
    timer_.reset();
    if (collection_)
      collection_->RemoveToast(this);
    message_center_->MarkSinglePopupAsShown(id(), false);
    SetBoundsWithAnimation(GetClosedToastBounds(bounds()));
  }

  void SetBoundsInstantly(gfx::Rect new_bounds) {
    if (new_bounds == bounds())
      return;

    origin_ = new_bounds.origin();
    if (!GetWidget())
      return;
    GetWidget()->SetBounds(new_bounds);
  }

  void SetBoundsWithAnimation(gfx::Rect new_bounds) {
    if (new_bounds == bounds())
      return;

    origin_ = new_bounds.origin();
    if (!GetWidget())
      return;

    // This picks up the current bounds, so if there was a previous animation
    // half-done, the next one will pick up from the current location.
    // This is the only place that should query current location of the Widget
    // on screen, the rest should refer to the bounds_.
    animated_bounds_start_ = GetWidget()->GetWindowBoundsInScreen();
    animated_bounds_end_ = new_bounds;

    if (collection_)
        collection_->IncrementDeferCounter();

    if (bounds_animation_.get())
      bounds_animation_->Stop();

    bounds_animation_.reset(new ui::SlideAnimation(this));
    closing_animation_ = (is_closing_ ? bounds_animation_.get() : NULL);
    bounds_animation_->Show();
  }

  void OnBoundsAnimationEndedOrCancelled(const ui::Animation* animation) {
    if (collection_)
      collection_->DecrementDeferCounter();

    if (is_closing_ && closing_animation_ == animation && GetWidget())
      GetWidget()->Close();
  }

  // ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE {
    if (animation == bounds_animation_.get()) {
      gfx::Rect current(animation->CurrentValueBetween(
          animated_bounds_start_, animated_bounds_end_));
      GetWidget()->SetBounds(current);
    }
  }
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE {
    OnBoundsAnimationEndedOrCancelled(animation);
  }
  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE {
    OnBoundsAnimationEndedOrCancelled(animation);
  }

  gfx::Point origin() { return origin_; }
  gfx::Rect bounds() { return gfx::Rect(origin_, preferred_size_); }
  const std::string& id() { return id_; }

 private:
  std::string id_;
  base::TimeDelta timeout_;
  base::TimeDelta passed_;
  base::Time start_time_;
  scoped_ptr<base::OneShotTimer<ToastContentsView> > timer_;
  base::WeakPtr<MessagePopupCollection> collection_;
  MessageCenter* message_center_;

  scoped_ptr<ui::SlideAnimation> bounds_animation_;
  bool is_animating_bounds_;
  gfx::Rect animated_bounds_start_;
  gfx::Rect animated_bounds_end_;
  // Started closing animation, will close at the end.
  bool is_closing_;
  // Closing animation - when it ends, close the widget. Weak, only used
  // for referential equality.
  ui::Animation* closing_animation_;

  // The stable origin of the toast on the screen. Note the current instant
  // origin may be different due to animations.
  gfx::Point origin_;
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(ToastContentsView);
};

MessagePopupCollection::MessagePopupCollection(gfx::NativeView parent,
                                               MessageCenter* message_center)
    : parent_(parent),
      message_center_(message_center),
      defer_counter_(0),
      user_is_closing_toasts_by_clicking_(false) {
  DCHECK(message_center_);
  defer_timer_.reset(new base::OneShotTimer<MessagePopupCollection>);
  DoUpdateIfPossible();
  message_center_->AddObserver(this);
}

MessagePopupCollection::~MessagePopupCollection() {
  message_center_->RemoveObserver(this);
  CloseAllWidgets();
}

void MessagePopupCollection::RemoveToast(ToastContentsView* toast) {
  for (Toasts::iterator iter = toasts_.begin(); iter != toasts_.end(); ++iter) {
    if ((*iter) == toast) {
      toasts_.erase(iter);
      break;
    }
  }
}

void MessagePopupCollection::UpdateWidgets() {
  NotificationList::PopupNotifications popups =
      message_center_->GetPopupNotifications();

  if (popups.empty()) {
    CloseAllWidgets();
    return;
  }

  gfx::Point base_position = GetWorkAreaBottomRight();
  int bottom = toasts_.empty() ?
      base_position.y() : toasts_.back()->origin().y();
  bottom -= kToastMargin;
  // Iterate in the reverse order to keep the oldest toasts on screen. Newer
  // items may be ignored if there are no room to place them.
  for (NotificationList::PopupNotifications::const_reverse_iterator iter =
           popups.rbegin(); iter != popups.rend(); ++iter) {
    if (HasToast((*iter)->id()))
      continue;

    MessageView* view =
        NotificationView::Create(*(*iter), message_center_, true);
    int view_height = GetToastSize(view).height();
    if (bottom - view_height - kToastMargin < 0) {
      delete view;
      break;
    }

    ToastContentsView* toast = new ToastContentsView(
        *iter, AsWeakPtr(), message_center_);
    toast->CreateWidget(parent_);
    toast->SetContents(view);
    toasts_.push_back(toast);

    toast->RevealWithAnimation(
        gfx::Point(base_position.x() - kToastMargin, bottom));
    bottom -= view_height + kToastMargin;

    message_center_->DisplayedNotification((*iter)->id());
  }
}

void MessagePopupCollection::OnMouseEntered() {
  for (Toasts::iterator iter = toasts_.begin(); iter != toasts_.end(); ++iter) {
    (*iter)->SuspendTimer();
  }
}

void MessagePopupCollection::OnMouseExited() {
  for (Toasts::iterator iter = toasts_.begin(); iter != toasts_.end(); ++iter) {
    (*iter)->StartTimer();
  }
}

void MessagePopupCollection::CloseAllWidgets() {
  for (Toasts::iterator iter = toasts_.begin(); iter != toasts_.end();) {
    // the toast can be removed from toasts_ during CloseWithAnimation().
    Toasts::iterator curiter = iter++;
    (*curiter)->CloseWithAnimation();
  }
  DCHECK(toasts_.empty());
}

gfx::Point MessagePopupCollection::GetWorkAreaBottomRight() {
  if (!work_area_.IsEmpty())
    return work_area_.bottom_right();

  if (!parent_) {
    // On Win+Aura, we don't have a parent since the popups currently show up
    // on the Windows desktop, not in the Aura/Ash desktop.  This code will
    // display the popups on the primary display.
    gfx::Screen* screen = gfx::Screen::GetNativeScreen();
    work_area_ = screen->GetPrimaryDisplay().work_area();
  } else {
    gfx::Screen* screen = gfx::Screen::GetScreenFor(parent_);
    work_area_ = screen->GetDisplayNearestWindow(parent_).work_area();
  }

  return work_area_.bottom_right();
}

void MessagePopupCollection::RepositionWidgets() {
  int bottom = GetWorkAreaBottomRight().y() - kToastMargin;
  for (Toasts::iterator iter = toasts_.begin(); iter != toasts_.end(); ++iter) {
    gfx::Rect bounds((*iter)->bounds());
    bounds.set_y(bottom - bounds.height());
    (*iter)->SetBoundsWithAnimation(bounds);
    bottom -= bounds.height() + kToastMargin;
  }
}

void MessagePopupCollection::RepositionWidgetsWithTarget() {
  if (toasts_.empty())
    return;

  if (toasts_.back()->origin().y() > target_top_edge_) {
    // No widgets are above, thus slides up the widgets.
    int slide_length =
        toasts_.back()->origin().y() - target_top_edge_;
    for (Toasts::iterator iter = toasts_.begin();
         iter != toasts_.end(); ++iter) {
      gfx::Rect bounds((*iter)->bounds());
      bounds.set_y(bounds.y() - slide_length);
      (*iter)->SetBoundsWithAnimation(bounds);
    }
  } else {
    Toasts::reverse_iterator iter = toasts_.rbegin();
    for (; iter != toasts_.rend(); ++iter) {
      if ((*iter)->origin().y() > target_top_edge_)
        break;
    }
    --iter;
    int slide_length = target_top_edge_ - (*iter)->origin().y();
    for (; ; --iter) {
      gfx::Rect bounds((*iter)->bounds());
      bounds.set_y(bounds.y() + slide_length);
      (*iter)->SetBoundsWithAnimation(bounds);

      if (iter == toasts_.rbegin())
        break;
    }
  }
}

void MessagePopupCollection::OnNotificationAdded(
    const std::string& notification_id) {
  DoUpdateIfPossible();
}

void MessagePopupCollection::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  // Find a toast.
  Toasts::iterator iter = toasts_.begin();
  for (; iter != toasts_.end(); ++iter) {
    if ((*iter)->id() == notification_id)
      break;
  }
  if (iter == toasts_.end())
    return;

  target_top_edge_ = (*iter)->bounds().y();
  (*iter)->CloseWithAnimation();
  if (by_user) {
    RepositionWidgetsWithTarget();
    // [Re] start a timeout after which the toasts re-position to their
    // normal locations after tracking the mouse pointer for easy deletion.
    // This provides a period of time when toasts are easy to remove because
    // they re-position themselves to have Close button right under the mouse
    // pointer. If the user continue to remove the toasts, the delay is reset.
    // Once user stopped removing the toasts, the toasts re-populate/rearrange
    // after the specified delay.
    if (!user_is_closing_toasts_by_clicking_) {
      user_is_closing_toasts_by_clicking_ = true;
      IncrementDeferCounter();
    }
    int update_timeout =
        toasts_.empty() ? kUpdateTimeoutOnUserActionEmptyMs :
                          kUpdateTimeoutOnUserActionMs;
    defer_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(update_timeout),
        this,
        &MessagePopupCollection::OnDeferTimerExpired);
  }
}

void MessagePopupCollection::OnDeferTimerExpired() {
  user_is_closing_toasts_by_clicking_ = false;
  DecrementDeferCounter();
}

void MessagePopupCollection::OnNotificationUpdated(
    const std::string& notification_id) {
  // Find a toast.
  Toasts::iterator toast_iter = toasts_.begin();
  for (; toast_iter != toasts_.end(); ++toast_iter) {
    if ((*toast_iter)->id() == notification_id)
      break;
  }
  if (toast_iter == toasts_.end())
    return;

  NotificationList::PopupNotifications notifications =
      message_center_->GetPopupNotifications();
  bool updated = false;

  for (NotificationList::PopupNotifications::iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    if ((*iter)->id() != notification_id)
      continue;

    MessageView* view = NotificationView::Create(
        *(*iter), message_center_, true);
    (*toast_iter)->SetContents(view);
    (*toast_iter)->ResetTimeout((*iter)->priority());
    updated = true;
  }

  // OnNotificationUpdated() can be called when a notification is excluded from
  // the popup notification list but still remains in the full notification
  // list. In that case the widget for the notification has to be closed here.
  if (!updated)
    (*toast_iter)->CloseWithAnimation();

  if (user_is_closing_toasts_by_clicking_)
    RepositionWidgetsWithTarget();
  else
    DoUpdateIfPossible();
}

void MessagePopupCollection::SetWorkAreaForTest(const gfx::Rect& work_area) {
  work_area_ = work_area;
}

bool MessagePopupCollection::HasToast(const std::string& notification_id) {
  for (Toasts::iterator iter = toasts_.begin(); iter != toasts_.end(); ++iter) {
    if ((*iter)->id() == notification_id)
      return true;
  }
  return false;
}

void MessagePopupCollection::IncrementDeferCounter() {
  defer_counter_++;
}

void MessagePopupCollection::DecrementDeferCounter() {
  defer_counter_--;
  DCHECK(defer_counter_ >= 0);
  DoUpdateIfPossible();
}

// This is the main sequencer of tasks. It does a step, then waits for
// all started transitions to play out before doing the next step.
// First, remove all expired toasts.
// Then, reposition widgets (the reposition on close happens before all
// deferred tasks are even able to run)
// Then, see if there is vacant space for new toasts.
void MessagePopupCollection::DoUpdateIfPossible() {
  if (defer_counter_ > 0)
    return;

  RepositionWidgets();

  if (defer_counter_ > 0)
    return;

  // Reposition could create extra space which allows additional widgets.
  UpdateWidgets();

  if (defer_counter_ > 0)
    return;

  // Test support. Quit the test run loop when no more updates are deferred,
  // meaining th echeck for updates did not cause anything to change so no new
  // transition animations were started.
  if (run_loop_for_test_.get())
    run_loop_for_test_->Quit();
}

views::Widget* MessagePopupCollection::GetWidgetForTest(const std::string& id) {
  for (Toasts::iterator iter = toasts_.begin(); iter != toasts_.end(); ++iter) {
    if ((*iter)->id() == id)
      return (*iter)->GetWidget();
  }
  return NULL;
}

void MessagePopupCollection::RunLoopForTest() {
  run_loop_for_test_.reset(new base::RunLoop());
  run_loop_for_test_->Run();
  run_loop_for_test_.reset();
}

gfx::Rect MessagePopupCollection::GetToastRectAt(size_t index) {
  DCHECK(defer_counter_ == 0) << "Fetching the bounds with animations active.";
  size_t i = 0;
  for (Toasts::iterator iter = toasts_.begin(); iter != toasts_.end(); ++iter) {
    if (i++ == index) {
      views::Widget* widget = (*iter)->GetWidget();
      if (widget)
        return widget->GetWindowBoundsInScreen();
      break;
    }
  }
  return gfx::Rect();
}

}  // namespace message_center
