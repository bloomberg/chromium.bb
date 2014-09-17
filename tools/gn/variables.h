// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_VARIABLES_H_
#define TOOLS_GN_VARIABLES_H_

#include <map>

#include "base/strings/string_piece.h"

namespace variables {

// Builtin vars ----------------------------------------------------------------

extern const char kBuildCpuArch[];
extern const char kBuildCpuArch_HelpShort[];
extern const char kBuildCpuArch_Help[];

extern const char kBuildOs[];
extern const char kBuildOs_HelpShort[];
extern const char kBuildOs_Help[];

extern const char kCpuArch[];
extern const char kCpuArch_HelpShort[];
extern const char kCpuArch_Help[];

extern const char kCurrentToolchain[];
extern const char kCurrentToolchain_HelpShort[];
extern const char kCurrentToolchain_Help[];

extern const char kDefaultToolchain[];
extern const char kDefaultToolchain_HelpShort[];
extern const char kDefaultToolchain_Help[];

extern const char kOs[];
extern const char kOs_HelpShort[];
extern const char kOs_Help[];

extern const char kPythonPath[];
extern const char kPythonPath_HelpShort[];
extern const char kPythonPath_Help[];

extern const char kRootBuildDir[];
extern const char kRootBuildDir_HelpShort[];
extern const char kRootBuildDir_Help[];

extern const char kRootGenDir[];
extern const char kRootGenDir_HelpShort[];
extern const char kRootGenDir_Help[];

extern const char kRootOutDir[];
extern const char kRootOutDir_HelpShort[];
extern const char kRootOutDir_Help[];

extern const char kTargetGenDir[];
extern const char kTargetGenDir_HelpShort[];
extern const char kTargetGenDir_Help[];

extern const char kTargetOutDir[];
extern const char kTargetOutDir_HelpShort[];
extern const char kTargetOutDir_Help[];

// Target vars -----------------------------------------------------------------

extern const char kAllDependentConfigs[];
extern const char kAllDependentConfigs_HelpShort[];
extern const char kAllDependentConfigs_Help[];

extern const char kAllowCircularIncludesFrom[];
extern const char kAllowCircularIncludesFrom_HelpShort[];
extern const char kAllowCircularIncludesFrom_Help[];

extern const char kArgs[];
extern const char kArgs_HelpShort[];
extern const char kArgs_Help[];

extern const char kCflags[];
extern const char kCflags_HelpShort[];
extern const char* kCflags_Help;

extern const char kCflagsC[];
extern const char kCflagsC_HelpShort[];
extern const char* kCflagsC_Help;

extern const char kCflagsCC[];
extern const char kCflagsCC_HelpShort[];
extern const char* kCflagsCC_Help;

extern const char kCflagsObjC[];
extern const char kCflagsObjC_HelpShort[];
extern const char* kCflagsObjC_Help;

extern const char kCflagsObjCC[];
extern const char kCflagsObjCC_HelpShort[];
extern const char* kCflagsObjCC_Help;

extern const char kCheckIncludes[];
extern const char kCheckIncludes_HelpShort[];
extern const char kCheckIncludes_Help[];

extern const char kCompleteStaticLib[];
extern const char kCompleteStaticLib_HelpShort[];
extern const char kCompleteStaticLib_Help[];

extern const char kConfigs[];
extern const char kConfigs_HelpShort[];
extern const char kConfigs_Help[];

extern const char kData[];
extern const char kData_HelpShort[];
extern const char kData_Help[];

extern const char kDataDeps[];
extern const char kDataDeps_HelpShort[];
extern const char kDataDeps_Help[];

extern const char kDefines[];
extern const char kDefines_HelpShort[];
extern const char kDefines_Help[];

extern const char kDepfile[];
extern const char kDepfile_HelpShort[];
extern const char kDepfile_Help[];

extern const char kDeps[];
extern const char kDeps_HelpShort[];
extern const char kDeps_Help[];

extern const char kForwardDependentConfigsFrom[];
extern const char kForwardDependentConfigsFrom_HelpShort[];
extern const char kForwardDependentConfigsFrom_Help[];

extern const char kIncludeDirs[];
extern const char kIncludeDirs_HelpShort[];
extern const char kIncludeDirs_Help[];

extern const char kInputs[];
extern const char kInputs_HelpShort[];
extern const char kInputs_Help[];

extern const char kLdflags[];
extern const char kLdflags_HelpShort[];
extern const char kLdflags_Help[];

extern const char kLibDirs[];
extern const char kLibDirs_HelpShort[];
extern const char kLibDirs_Help[];

extern const char kLibs[];
extern const char kLibs_HelpShort[];
extern const char kLibs_Help[];

extern const char kOutputExtension[];
extern const char kOutputExtension_HelpShort[];
extern const char kOutputExtension_Help[];

extern const char kOutputName[];
extern const char kOutputName_HelpShort[];
extern const char kOutputName_Help[];

extern const char kOutputs[];
extern const char kOutputs_HelpShort[];
extern const char kOutputs_Help[];

extern const char kPublic[];
extern const char kPublic_HelpShort[];
extern const char kPublic_Help[];

extern const char kPublicConfigs[];
extern const char kPublicConfigs_HelpShort[];
extern const char kPublicConfigs_Help[];

extern const char kPublicDeps[];
extern const char kPublicDeps_HelpShort[];
extern const char kPublicDeps_Help[];

extern const char kScript[];
extern const char kScript_HelpShort[];
extern const char kScript_Help[];

extern const char kSources[];
extern const char kSources_HelpShort[];
extern const char kSources_Help[];

extern const char kTestonly[];
extern const char kTestonly_HelpShort[];
extern const char kTestonly_Help[];

extern const char kVisibility[];
extern const char kVisibility_HelpShort[];
extern const char kVisibility_Help[];

// -----------------------------------------------------------------------------

struct VariableInfo {
  VariableInfo();
  VariableInfo(const char* in_help_short,
               const char* in_help);

  const char* help_short;
  const char* help;
};

typedef std::map<base::StringPiece, VariableInfo> VariableInfoMap;

// Returns the built-in readonly variables.
// Note: this is used only for help so this getter is not threadsafe.
const VariableInfoMap& GetBuiltinVariables();

// Returns the variables used by target generators.
// Note: this is used only for help so this getter is not threadsafe.
const VariableInfoMap& GetTargetVariables();

}  // namespace variables

#endif  // TOOLS_GN_VARIABLES_H_
