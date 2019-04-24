// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_STRINGS_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_STRINGS_H_

#include <windows.h>

namespace chrome_cleaner {

// Command line switches.

// The switch to activate the sleeping action for specified delay in minutes
// before killing the process.
extern const char kTestSleepMinutesSwitch[];

// The switch to signal the event with the name given as a switch value.
extern const char kTestEventToSignal[];

// Test the overwrite of the ZoneIdentifier.
extern const char kTestForceOverwriteZoneIdentifier[];

// Test service properties.

// The name of the service used for tests.
extern const wchar_t kServiceName[];
// The description of the service used for tests.
extern const wchar_t kServiceDescription[];

// The sleep time in ms between each poll attempt to get information about a
// service.
extern const unsigned int kServiceQueryWaitTimeMs;
// The number of attempts to contact a service.
extern const int kServiceQueryRetry;

// A valid uft8 name for a file.
extern const wchar_t kValidUtf8Name[];

// An invalid uft8 name for a file.
extern const wchar_t kInvalidUtf8Name[];

// The test data written to file.
const int kFileContentSize = 26;
extern const char kFileContent[kFileContentSize];

// GUIDs for tests.
extern const GUID kGUID1;
extern const GUID kGUID2;
extern const GUID kGUID3;
extern const wchar_t kGUID1Str[];
extern const wchar_t kGUID2Str[];
extern const wchar_t kGUID3Str[];

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_STRINGS_H_
