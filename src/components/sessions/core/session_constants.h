// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SESSION_CONSTANTS_H_
#define COMPONENTS_SESSIONS_CORE_SESSION_CONSTANTS_H_

#include "base/files/file_path.h"
#include "components/sessions/core/sessions_export.h"

namespace sessions {

// Directory under the profile directory to store session data.
// Added in Chrome 85.
extern const base::FilePath::StringPieceType SESSIONS_EXPORT kSessionsDirectory;

// File name prefix for a type of TAB.
// Added in Chrome 85.
extern const base::FilePath::StringPieceType SESSIONS_EXPORT
    kTabSessionFileNamePrefix;

// File name prefix for a type of SESSION.
// Added in Chrome 85.
extern const base::FilePath::StringPieceType SESSIONS_EXPORT
    kSessionFileNamePrefix;

// Legacy file names (current and previous) for a type of TAB.
// Used before Chrome 85.
extern const base::FilePath::StringPieceType SESSIONS_EXPORT
    kLegacyCurrentTabSessionFileName;
extern const base::FilePath::StringPieceType SESSIONS_EXPORT
    kLegacyLastTabSessionFileName;

// Legacy file names (current and previous) for a type of SESSION.
// Used before Chrome 85.
extern const base::FilePath::StringPieceType SESSIONS_EXPORT
    kLegacyCurrentSessionFileName;
extern const base::FilePath::StringPieceType SESSIONS_EXPORT
    kLegacyLastSessionFileName;

// The maximum number of navigation entries in each direction to persist.
extern const int SESSIONS_EXPORT gMaxPersistNavigationCount;

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_SESSION_CONSTANTS_H_
