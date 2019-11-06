// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CHROME_ELF_INIT_H_
#define CHROME_BROWSER_WIN_CHROME_ELF_INIT_H_

// Field trial name and full name for the blacklist disabled group.
extern const char kBrowserBlacklistTrialName[];
extern const char kBrowserBlacklistTrialDisabledGroupName[];

// Prepare any initialization code for Chrome Elf's setup (This will generally
// only affect future runs since Chrome Elf is already setup by this point).
void InitializeChromeElf();

// Set the required state for an enabled browser blacklist.
void BrowserBlacklistBeaconSetup();

#endif  // CHROME_BROWSER_WIN_CHROME_ELF_INIT_H_
