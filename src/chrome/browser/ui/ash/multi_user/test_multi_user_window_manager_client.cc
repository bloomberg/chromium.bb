// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/test_multi_user_window_manager_client.h"

#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/account_id/account_id.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

TestMultiUserWindowManagerClient::TestMultiUserWindowManagerClient(
    Browser* visiting_browser,
    const AccountId& desktop_owner)
    : browser_window_(visiting_browser->window()->GetNativeWindow()),
      browser_owner_(multi_user_util::GetAccountIdFromProfile(
          visiting_browser->profile())),
      desktop_owner_(desktop_owner),
      created_window_(NULL),
      created_window_shown_for_(browser_owner_),
      current_account_id_(desktop_owner) {
  // Register this object with the system (which will take ownership). It will
  // be deleted by ChromeLauncherController::~ChromeLauncherController().
  MultiUserWindowManagerClient::SetInstanceForTest(this);
}

TestMultiUserWindowManagerClient::~TestMultiUserWindowManagerClient() {
  // This object is owned by the MultiUserWindowManager since the
  // SetInstanceForTest call. As such no uninstall is required.
}

void TestMultiUserWindowManagerClient::SetWindowOwner(
    aura::Window* window,
    const AccountId& account_id) {
  NOTREACHED();
}

const AccountId& TestMultiUserWindowManagerClient::GetWindowOwner(
    const aura::Window* window) const {
  // No matter which window will get queried - all browsers belong to the
  // original browser's user.
  return browser_owner_;
}

void TestMultiUserWindowManagerClient::ShowWindowForUser(
    aura::Window* window,
    const AccountId& account_id) {
  // This class is only able to handle one additional window <-> user
  // association beside the creation parameters.
  // If no association has yet been requested remember it now.
  if (browser_owner_ != account_id)
    DCHECK(!created_window_);
  created_window_ = window;
  created_window_shown_for_ = account_id;

  if (browser_window_ == window)
    desktop_owner_ = account_id;

  if (account_id == current_account_id_)
    return;

  // Change the visibility of the window to update the view recursively.
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  widget->Hide();
  widget->Show();
  current_account_id_ = account_id;
}

bool TestMultiUserWindowManagerClient::AreWindowsSharedAmongUsers() const {
  return browser_owner_ != desktop_owner_;
}

void TestMultiUserWindowManagerClient::GetOwnersOfVisibleWindows(
    std::set<AccountId>* account_ids) const {}

bool TestMultiUserWindowManagerClient::IsWindowOnDesktopOfUser(
    aura::Window* window,
    const AccountId& account_id) const {
  return GetUserPresentingWindow(window) == account_id;
}

const AccountId& TestMultiUserWindowManagerClient::GetUserPresentingWindow(
    const aura::Window* window) const {
  if (window == browser_window_)
    return desktop_owner_;
  if (created_window_ && window == created_window_)
    return created_window_shown_for_;
  // We can come here before the window gets registered.
  return browser_owner_;
}

void TestMultiUserWindowManagerClient::AddUser(
    content::BrowserContext* profile) {}

void TestMultiUserWindowManagerClient::AddObserver(Observer* observer) {}

void TestMultiUserWindowManagerClient::RemoveObserver(Observer* observer) {}
