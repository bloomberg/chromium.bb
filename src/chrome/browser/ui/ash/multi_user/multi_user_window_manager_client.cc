// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_client.h"

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_client_impl.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_client_stub.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_info.h"
#include "components/user_manager/user_manager.h"

namespace {
MultiUserWindowManagerClient* g_multi_user_window_manager_instance = nullptr;
}  // namespace

// static
MultiUserWindowManagerClient* MultiUserWindowManagerClient::GetInstance() {
  return g_multi_user_window_manager_instance;
}

MultiUserWindowManagerClient* MultiUserWindowManagerClient::CreateInstance() {
  DCHECK(!g_multi_user_window_manager_instance);
  if (SessionControllerClient::IsMultiProfileAvailable()) {
    MultiUserWindowManagerClientImpl* manager =
        new MultiUserWindowManagerClientImpl(
            user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
    g_multi_user_window_manager_instance = manager;
    manager->Init();
  } else {
    // Use a stub when multi-profile is not available.
    g_multi_user_window_manager_instance =
        new MultiUserWindowManagerClientStub();
  }

  return g_multi_user_window_manager_instance;
}

// static
bool MultiUserWindowManagerClient::ShouldShowAvatar(aura::Window* window) {
  // Session restore can open a window for the first user before the instance
  // is created.
  if (!g_multi_user_window_manager_instance)
    return false;

  // Show the avatar icon if the window is on a different desktop than the
  // window's owner's desktop. The stub implementation does the right thing
  // for single-user mode.
  return !g_multi_user_window_manager_instance->IsWindowOnDesktopOfUser(
      window, g_multi_user_window_manager_instance->GetWindowOwner(window));
}

// static
void MultiUserWindowManagerClient::DeleteInstance() {
  DCHECK(g_multi_user_window_manager_instance);
  delete g_multi_user_window_manager_instance;
  g_multi_user_window_manager_instance = nullptr;
}

// static
void MultiUserWindowManagerClient::SetInstanceForTest(
    MultiUserWindowManagerClient* instance) {
  if (g_multi_user_window_manager_instance)
    DeleteInstance();
  g_multi_user_window_manager_instance = instance;
}
