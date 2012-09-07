// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_METRO_DRIVER_SECONDARY_TILE_H_
#define CHROME_BROWSER_UI_METRO_DRIVER_SECONDARY_TILE_H_

#include "base/string16.h"

extern "C" __declspec(dllexport)
BOOL MetroIsPinnedToStartScreen(const string16& url);

extern "C" __declspec(dllexport)
void MetroTogglePinnedToStartScreen(const string16& title, const string16& url);

#endif  // CHROME_BROWSER_UI_METRO_DRIVER_SECONDARY_TILE_H_

