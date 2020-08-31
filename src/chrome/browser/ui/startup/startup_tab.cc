// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab.h"

StartupTab::StartupTab()
    : is_pinned(true) {
}

StartupTab::StartupTab(const GURL& url, bool is_pinned)
    : url(url), is_pinned(is_pinned) {}

StartupTab::~StartupTab() {
}
