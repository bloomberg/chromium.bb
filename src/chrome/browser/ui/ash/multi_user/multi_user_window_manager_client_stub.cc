// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_client_stub.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/account_id/account_id.h"

MultiUserWindowManagerClientStub::MultiUserWindowManagerClientStub() {}

MultiUserWindowManagerClientStub::~MultiUserWindowManagerClientStub() {}

void MultiUserWindowManagerClientStub::SetWindowOwner(
    aura::Window* window,
    const AccountId& account_id) {
  NOTIMPLEMENTED();
}

const AccountId& MultiUserWindowManagerClientStub::GetWindowOwner(
    const aura::Window* window) const {
  return EmptyAccountId();
}

void MultiUserWindowManagerClientStub::ShowWindowForUser(
    aura::Window* window,
    const AccountId& account_id) {
  NOTIMPLEMENTED();
}

bool MultiUserWindowManagerClientStub::AreWindowsSharedAmongUsers() const {
  return false;
}

void MultiUserWindowManagerClientStub::GetOwnersOfVisibleWindows(
    std::set<AccountId>* account_ids) const {}

bool MultiUserWindowManagerClientStub::IsWindowOnDesktopOfUser(
    aura::Window* window,
    const AccountId& account_id) const {
  return true;
}

const AccountId& MultiUserWindowManagerClientStub::GetUserPresentingWindow(
    const aura::Window* window) const {
  return EmptyAccountId();
}

void MultiUserWindowManagerClientStub::AddUser(
    content::BrowserContext* context) {
  NOTIMPLEMENTED();
}

void MultiUserWindowManagerClientStub::AddObserver(Observer* observer) {
  NOTIMPLEMENTED();
}

void MultiUserWindowManagerClientStub::RemoveObserver(Observer* observer) {
  NOTIMPLEMENTED();
}
