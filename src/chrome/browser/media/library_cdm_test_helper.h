// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_LIBRARY_CDM_TEST_HELPER_H_
#define CHROME_BROWSER_MEDIA_LIBRARY_CDM_TEST_HELPER_H_

#include <string>

#include "base/files/file_path.h"

namespace base {
class CommandLine;
class Token;
}

// Registers ClearKeyCdm in |command_line|.
void RegisterClearKeyCdm(base::CommandLine* command_line,
                         bool use_wrong_cdm_path = false);

bool IsLibraryCdmRegistered(const base::Token& cdm_guid);

#endif  // CHROME_BROWSER_MEDIA_LIBRARY_CDM_TEST_HELPER_H_
