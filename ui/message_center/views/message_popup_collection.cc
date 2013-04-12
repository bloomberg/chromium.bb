// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_popup_collection.h"

#include <set>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/timer.h"
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

class ToastContentsView : public views::WidgetDelegateView {
 public:
  ToastContentsView(const Notification* notification,
                    base::WeakPtr<MessagePopupCollection> collection,
                    MessageCenter* message_center)
      : id_(notification->id()),
        collection_(collection),
        message_center_(message_center) {
    DCHECK(collection_);

    set_notify_enter_exit_on_child(true);
    // Sets the transparent background. Then, when the message view is slid out,
    // the whole toast seems to slide although the actual bound of the widget
    // remains. This is hacky but easier to keep the consistency.
    set_background(views::Background::CreateSolidBackground(0, 0, 0, 0));

    int seconds = kAutocloseDefaultDelaySeconds;
    if (notification->priority() > DEFAULT_PRIORITY)
      seconds = kAutocloseHighPriorityDelaySeconds;
    delay_ = base::TimeDelta::FromSeconds(seconds);

    // Creates the timer only when it does the timeout (i.e. not never-timeout).
    if (!notification->never_timeout())
      timer_.reset(new base::OneShotTimer<views::Widget>);
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
    views::Widget* widget = GetWidget();
    if (widget) {
      gfx::Rect bounds = widget->GetWindowBoundsInScreen();
      bounds.set_width(kNotificationWidth);
      bounds.set_height(view->GetHeightForWidth(kNotificationWidth));
      widget->SetBounds(bounds);
    }
    Layout();
  }

  void SuspendTimer() {
    if (timer_.get())
      timer_->Stop();
  }

  void RestartTimer() {
    if (!timer_.get())
      return;

    delay_ -= base::Time::Now() - start_time_;
    if (delay_ < base::TimeDelta())
      GetWidget()->Close();
    else
      StartTimer();
  }

  void StartTimer() {
    if (!timer_.get())
      return;

    start_time_ = base::Time::Now();
    timer_->Start(FROM_HERE,
                  delay_,
                  base::Bind(&views::Widget::Close,
                             base::Unretained(GetWidget())));
  }

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }

  virtual void WindowClosing() OVERRIDE {
    if (timer_.get() && timer_->IsRunning())
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
    if (child_count() > 0)
      child_at(0)->SetBounds(x(), y(), width(), height());
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    if (child_count() == 0)
      return gfx::Size();

    return gfx::Size(kNotificationWidth,
                     child_at(0)->GetHeightForWidth(kNotificationWidth));
  }

 private:
  std::string id_;
  base::TimeDelta delay_;
  base::Time start_time_;
  scoped_ptr<base::OneShotTimer<views::Widget> > timer_;
  base::WeakPtr<MessagePopupCollection> collection_;
  MessageCenter* message_center_;

  DISALLOW_COPY_AND_ASSIGN(ToastContentsView);
};

MessagePopupCollection::MessagePopupCollection(gfx::NativeView parent,
                                               MessageCenter* message_center)
    : parent_(parent),
      message_center_(message_center) {
  DCHECK(message_center_);

  UpdateWidgets();
  message_center_->AddObserver(this);
}

MessagePopupCollection::~MessagePopupCollection() {
  message_center_->RemoveObserver(this);
  CloseAllWidgets();
}

void MessagePopupCollection::UpdateWidgets() {
  NotificationList::PopupNotifications popups =
      message_center_->GetPopupNotifications();

  if (popups.empty()) {
    CloseAllWidgets();
    return;
  }

  gfx::Point base_position = GetWorkAreaBottomRight();
  int bottom = widgets_.empty() ?
      base_position.y() : widgets_.back()->GetWindowBoundsInScreen().y();
  bottom -= kMarginBetweenItems;
  int left = base_position.x() - kNotificationWidth - kMarginBetweenItems;
  // Iterate in the reverse order to keep the oldest toasts on screen. Newer
  // items may be ignored if there are no room to place them.
  for (NotificationList::PopupNotifications::const_reverse_iterator iter =
           popups.rbegin(); iter != popups.rend(); ++iter) {
    MessageView* view =
        NotificationView::Create(*(*iter), message_center_, true);
    int view_height = view->GetHeightForWidth(kNotificationWidth);
    if (bottom - view_height - kMarginBetweenItems < 0) {
      delete view;
      break;
    }

    if (toasts_.find((*iter)->id()) != toasts_.end()) {
      delete view;
      continue;
    }

    ToastContentsView* toast = new ToastContentsView(
        *iter, AsWeakPtr(), message_center_);
    views::Widget* widget = toast->CreateWidget(parent_);
    toast->SetContents(view);
    widget->AddObserver(this);
    toast->StartTimer();
    toasts_[(*iter)->id()] = toast;
    widgets_.push_back(widget);

    // Place/move the toast widgets. Currently it stacks the widgets from the
    // right-bottom of the work area.
    // TODO(mukai): allow to specify the placement policy from outside of this
    // class. The policy should be specified from preference on Windows, or
    // the launcher alignment on ChromeOS.
    gfx::Rect bounds(widget->GetWindowBoundsInScreen());
    bounds.set_origin(gfx::Point(left, bottom - bounds.height()));
    widget->SetBounds(bounds);
    widget->Show();
    bottom -= view_height + kMarginBetweenItems;
  }
}

void MessagePopupCollection::OnMouseEntered() {
  for (ToastContainer::iterator iter = toasts_.begin();
       iter != toasts_.end(); ++iter) {
    iter->second->SuspendTimer();
  }
}

void MessagePopupCollection::OnMouseExited() {
  for (ToastContainer::iterator iter = toasts_.begin();
       iter != toasts_.end(); ++iter) {
    iter->second->RestartTimer();
  }
  RepositionWidgets();
  // Reposition could create extra space which allows additional widgets.
  UpdateWidgets();
}

void MessagePopupCollection::CloseAllWidgets() {
  for (ToastContainer::iterator iter = toasts_.begin();
       iter != toasts_.end(); ++iter) {
    iter->second->SuspendTimer();
    views::Widget* widget = iter->second->GetWidget();
    widget->RemoveObserver(this);
    widget->Close();
  }
  toasts_.clear();
  widgets_.clear();
}

void MessagePopupCollection::OnWidgetDestroying(views::Widget* widget) {
  widget->RemoveObserver(this);
  for (ToastContainer::iterator iter = toasts_.begin();
       iter != toasts_.end(); ++iter) {
    if (iter->second->GetWidget() == widget) {
      message_center_->MarkSinglePopupAsShown(iter->first, false);
      toasts_.erase(iter);
      break;
    }
  }
  widgets_.erase(std::find(widgets_.begin(), widgets_.end(), widget));
  RepositionWidgets();
  UpdateWidgets();
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
  int bottom = GetWorkAreaBottomRight().y() - kMarginBetweenItems;
  for (std::list<views::Widget*>::iterator iter = widgets_.begin();
       iter != widgets_.end(); ++iter) {
    gfx::Rect bounds((*iter)->GetWindowBoundsInScreen());
    bounds.set_y(bottom - bounds.height());
    (*iter)->SetBounds(bounds);
    bottom -= bounds.height() + kMarginBetweenItems;
  }
}

void MessagePopupCollection::RepositionWidgetsWithTarget(
    const gfx::Rect& target_bounds) {
  if (widgets_.empty())
    return;

  if (widgets_.back()->GetWindowBoundsInScreen().y() > target_bounds.y()) {
    // No widgets are above, thus slides up the widgets.
    int slide_length =
        widgets_.back()->GetWindowBoundsInScreen().y() - target_bounds.y();
    for (std::list<views::Widget*>::iterator iter = widgets_.begin();
         iter != widgets_.end(); ++iter) {
      gfx::Rect bounds((*iter)->GetWindowBoundsInScreen());
      bounds.set_y(bounds.y() - slide_length);
      (*iter)->SetBounds(bounds);
    }
  } else {
    std::list<views::Widget*>::reverse_iterator iter = widgets_.rbegin();
    for (; iter != widgets_.rend(); ++iter) {
      if ((*iter)->GetWindowBoundsInScreen().y() > target_bounds.y())
        break;
    }
    --iter;
    int slide_length =
        target_bounds.y() - (*iter)->GetWindowBoundsInScreen().y();
    for (; ; --iter) {
      gfx::Rect bounds((*iter)->GetWindowBoundsInScreen());
      bounds.set_y(bounds.y() + slide_length);
      (*iter)->SetBounds(bounds);

      if (iter == widgets_.rbegin())
        break;
    }
  }
}

void MessagePopupCollection::OnNotificationAdded(
    const std::string& notification_id) {
  UpdateWidgets();
}

void MessagePopupCollection::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  ToastContainer::iterator iter = toasts_.find(notification_id);
  if (iter == toasts_.end())
    return;

  views::Widget* widget = iter->second->GetWidget();
  gfx::Rect removed_bounds = widget->GetWindowBoundsInScreen();
  widget->RemoveObserver(this);
  widget->Close();
  widgets_.erase(std::find(widgets_.begin(), widgets_.end(), widget));
  toasts_.erase(iter);
  bool widgets_went_empty = widgets_.empty();
  if (by_user)
    RepositionWidgetsWithTarget(removed_bounds);
  else
    RepositionWidgets();

  // A notification removal may create extra space which allows appearing
  // other notifications.
  UpdateWidgets();

  // Also, if the removed notification is the last one but removing that enables
  // other notifications appearing, the newly created widgets also have to be
  // repositioned.
  if (by_user && widgets_went_empty)
    RepositionWidgetsWithTarget(removed_bounds);
}

void MessagePopupCollection::OnNotificationUpdated(
    const std::string& notification_id) {
  ToastContainer::iterator toast_iter = toasts_.find(notification_id);
  if (toast_iter == toasts_.end())
    return;

  NotificationList::Notifications notifications =
      message_center_->GetNotifications();
  bool updated = false;
  for (NotificationList::Notifications::iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    if ((*iter)->id() != notification_id)
      continue;

    MessageView* view = NotificationView::Create(
        *(*iter), message_center_, true);
    toast_iter->second->SetContents(view);
    updated = true;
  }

  if (updated) {
    RepositionWidgets();
    // Reposition could create extra space which allows additional widgets.
    UpdateWidgets();
  }
}

void MessagePopupCollection::SetWorkAreaForTest(const gfx::Rect& work_area) {
  work_area_ = work_area;
}

}  // namespace message_center
