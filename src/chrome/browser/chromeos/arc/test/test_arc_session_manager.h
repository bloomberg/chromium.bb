// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TEST_TEST_ARC_SESSION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TEST_TEST_ARC_SESSION_MANAGER_H_

#include <memory>

namespace base {
class FilePath;
}  // namespace base

namespace arc {

class ArcSessionManager;
class ArcSessionRunner;

// Creates an ArcSessionManager object that is suitable for unit testing.
// Unlike the regular one, this function's behaves as if the property files
// has already successfully been done.
std::unique_ptr<ArcSessionManager> CreateTestArcSessionManager(
    std::unique_ptr<ArcSessionRunner> arc_session_runner);

// Does something similar to CreateTestArcSessionManager(), but for an existing
// object. This function is useful for ARC browser_tests where ArcSessionManager
// object is (re)created with ArcServiceLauncher::ResetForTesting(). This
// function automatically starts prop files expansion and it succeeds as long as
// |temp_dir| is writable.
bool ExpandPropertyFilesForTesting(ArcSessionManager* arc_session_manager,
                                   const base::FilePath& temp_dir);

}  // namespace arc

#endif  //  CHROME_BROWSER_CHROMEOS_ARC_TEST_TEST_ARC_SESSION_MANAGER_H_
