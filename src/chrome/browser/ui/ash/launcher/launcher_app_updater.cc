// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_app_updater.h"

LauncherAppUpdater::LauncherAppUpdater(Delegate* delegate,
                                       content::BrowserContext* browser_context)
    : delegate_(delegate), browser_context_(browser_context) {}

LauncherAppUpdater::~LauncherAppUpdater() {}
