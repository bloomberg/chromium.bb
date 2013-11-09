// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_switches.h"

#include "base/command_line.h"

namespace app_list {
namespace switches {

// If set, folder will be enabled in app list UI.
const char kEnableFolderUI[] = "enable-app-list-folder-ui";

bool IsFolderUIEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(kEnableFolderUI);
}

}  // namespcae switches
}  // namespace app_list
