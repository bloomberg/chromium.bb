//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system_utils.h: declaration of OS-specific utility functions

#ifndef COMMON_SYSTEM_UTILS_H_
#define COMMON_SYSTEM_UTILS_H_

#include "common/angleutils.h"
#include "common/Optional.h"

namespace angle
{

const char *GetExecutablePath();
const char *GetExecutableDirectory();
const char *GetSharedLibraryExtension();
Optional<std::string> GetCWD();
bool SetCWD(const char *dirName);
bool SetEnvironmentVar(const char *variableName, const char *value);
bool UnsetEnvironmentVar(const char *variableName);
std::string GetEnvironmentVar(const char *variableName);
const char *GetPathSeparator();
bool PrependPathToEnvironmentVar(const char *variableName, const char *path);

}  // namespace angle

#endif  // COMMON_SYSTEM_UTILS_H_
