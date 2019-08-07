// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TABLET_MODE_CLIENT_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_TABLET_MODE_CLIENT_TEST_UTIL_H_

namespace test {

// Enables or disables the tablet mode and waits to until the change has made
// its way back into Chrome (from Ash). Should only be called to toggle the
// current mode.
void SetAndWaitForTabletMode(bool enabled);

}  // namespace test

#endif  // CHROME_BROWSER_UI_ASH_TABLET_MODE_CLIENT_TEST_UTIL_H_
