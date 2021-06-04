// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_TERMINAL_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_TERMINAL_SYSTEM_WEB_APP_INFO_H_

#include <memory>

#include "ui/gfx/geometry/rect.h"

class Browser;
struct WebApplicationInfo;

// Returns a WebApplicationInfo used to install the app.
std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForTerminalSystemWebApp();

// Returns the default bounds.
gfx::Rect GetDefaultBoundsForTerminal(Browser* browser);

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_TERMINAL_SYSTEM_WEB_APP_INFO_H_
