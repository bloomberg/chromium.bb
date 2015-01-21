// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayModeProperties_h
#define DisplayModeProperties_h

namespace blink {

enum DisplayMode {
    DisplayModeUndefined, // User for override setting (ie. not set).
    DisplayModeBrowser,
    DisplayModeMinimalUi,
    DisplayModeStandalone,
    DisplayModeFullscreen
};

}

#endif
