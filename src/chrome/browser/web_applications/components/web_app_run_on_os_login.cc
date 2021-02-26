// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_run_on_os_login.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace web_app {

namespace {

void RegisterRunOnOsLoginAndPostCallback(RegisterRunOnOsLoginCallback callback,
                                         const ShortcutInfo& shortcut_info) {
  bool run_on_os_login_registered =
      internals::RegisterRunOnOsLogin(shortcut_info);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_on_os_login_registered));
}
}  // namespace

namespace internals {

#if !defined(OS_WIN)
// TODO(crbug.com/897302): This boilerplate function is used for platforms
// other than Windows, currently the feature is only supported in Windows.
bool RegisterRunOnOsLogin(const ShortcutInfo& shortcut_info) {
  return false;
}

// TODO(crbug.com/897302): This boilerplate function is used for platforms
// other than Windows, currently the feature is only supported in Windows.
bool UnregisterRunOnOsLogin(const base::FilePath& profile_path,
                            const base::string16& shortcut_title) {
  return true;
}
#endif

}  // namespace internals

void ScheduleRegisterRunOnOsLogin(std::unique_ptr<ShortcutInfo> shortcut_info,
                                  RegisterRunOnOsLoginCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  internals::PostShortcutIOTask(
      base::BindOnce(&RegisterRunOnOsLoginAndPostCallback, std::move(callback)),
      std::move(shortcut_info));
}

void ScheduleUnregisterRunOnOsLogin(const base::FilePath& profile_path,
                                    const base::string16& shortcut_title,
                                    UnregisterRunOnOsLoginCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  internals::GetShortcutIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&internals::UnregisterRunOnOsLogin, profile_path,
                     shortcut_title),
      std::move(callback));
}

}  // namespace web_app
