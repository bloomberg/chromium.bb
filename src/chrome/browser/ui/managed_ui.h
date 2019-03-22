// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MANAGED_UI_H_
#define CHROME_BROWSER_UI_MANAGED_UI_H_

class Profile;

namespace chrome {

// Returns true if a 'Managed by your organization' message should appear in
// Chrome's App Menu, and on the following chrome:// pages:
// - chrome://bookmarks
// - chrome://downloads
// - chrome://extensions
// - chrome://history
// - chrome://settings
//
// N.B.: This is independent of Chrome OS's system tray message for enterprise
// users.
bool ShouldDisplayManagedUi(Profile* profile);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_MANAGED_UI_H_
