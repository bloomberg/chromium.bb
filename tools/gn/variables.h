// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_VARIABLES_H_
#define TOOLS_GN_VARIABLES_H_

#include <map>

#include "base/strings/string_piece.h"

namespace variables {

// Builtin vars ----------------------------------------------------------------

extern const char kComponentMode[];
extern const char kComponentMode_HelpShort[];
extern const char kComponentMode_Help[];

extern const char kCurrentToolchain[];
extern const char kCurrentToolchain_HelpShort[];
extern const char kCurrentToolchain_Help[];

extern const char kDefaultToolchain[];
extern const char kDefaultToolchain_HelpShort[];
extern const char kDefaultToolchain_Help[];

extern const char kIsLinux[];
extern const char kIsLinux_HelpShort[];
extern const char kIsLinux_Help[];

extern const char kIsMac[];
extern const char kIsMac_HelpShort[];
extern const char kIsMac_Help[];

extern const char kIsPosix[];
extern const char kIsPosix_HelpShort[];
extern const char kIsPosix_Help[];

extern const char kIsWin[];
extern const char kIsWin_HelpShort[];
extern const char kIsWin_Help[];

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

extern const char kConfigs[];
extern const char kConfigs_HelpShort[];
extern const char kConfigs_Help[];

extern const char kData[];
extern const char kData_HelpShort[];
extern const char kData_Help[];

extern const char kDatadeps[];
extern const char kDatadeps_HelpShort[];
extern const char kDatadeps_Help[];

extern const char kDefines[];
extern const char kDefines_HelpShort[];
extern const char kDefines_Help[];

extern const char kDeps[];
extern const char kDeps_HelpShort[];
extern const char kDeps_Help[];

extern const char kDirectDependentConfigs[];
extern const char kDirectDependentConfigs_HelpShort[];
extern const char kDirectDependentConfigs_Help[];

extern const char kExternal[];
extern const char kExternal_HelpShort[];
extern const char kExternal_Help[];

extern const char kForwardDependentConfigsFrom[];
extern const char kForwardDependentConfigsFrom_HelpShort[];
extern const char kForwardDependentConfigsFrom_Help[];

extern const char kHardDep[];
extern const char kHardDep_HelpShort[];
extern const char kHardDep_Help[];

extern const char kIncludes[];
extern const char kIncludes_HelpShort[];
extern const char kIncludes_Help[];

extern const char kLdflags[];
extern const char kLdflags_HelpShort[];
extern const char kLdflags_Help[];

extern const char kOutputName[];
extern const char kOutputName_HelpShort[];
extern const char kOutputName_Help[];

extern const char kOutputs[];
extern const char kOutputs_HelpShort[];
extern const char kOutputs_Help[];

extern const char kScript[];
extern const char kScript_HelpShort[];
extern const char kScript_Help[];

extern const char kSourcePrereqs[];
extern const char kSourcePrereqs_HelpShort[];
extern const char kSourcePrereqs_Help[];

extern const char kSources[];
extern const char kSources_HelpShort[];
extern const char kSources_Help[];

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
