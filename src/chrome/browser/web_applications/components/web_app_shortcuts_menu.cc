// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_shortcuts_menu.h"

#include "base/logging.h"
#include "build/build_config.h"

namespace web_app {

#if !defined(OS_WIN)
bool ShouldRegisterShortcutsMenuWithOs() {
  return false;
}

void RegisterShortcutsMenuWithOs(
    const base::FilePath& shortcut_data_dir,
    const AppId& app_id,
    const base::FilePath& profile_path,
    const std::vector<WebApplicationShortcutInfo>& shortcuts) {
  NOTIMPLEMENTED();
  DCHECK(ShouldRegisterShortcutsMenuWithOs());
}

void UnregisterShortcutsMenuWithOs(const AppId& app_id,
                                   const base::FilePath& profile_path) {
  NOTIMPLEMENTED();
  DCHECK(ShouldRegisterShortcutsMenuWithOs());
}
#endif  // !defined(OS_WIN)

}  // namespace web_app
