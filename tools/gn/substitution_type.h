// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SUBSTITUTION_TYPE_H_
#define TOOLS_GN_SUBSTITUTION_TYPE_H_

#include <vector>

class Err;
class ParseNode;

// Keep kSubstitutionNames, kSubstitutionNinjaNames and the
// IsValid*Substutition functions in sync if you change anything here.
enum SubstitutionType {
  SUBSTITUTION_LITERAL = 0,

  // The index of the first pattern. To loop overal all patterns, go from here
  // until NUM_TYPES.
  SUBSTITUTION_FIRST_PATTERN,

  SUBSTITUTION_SOURCE = SUBSTITUTION_FIRST_PATTERN,  // {{source}}
  SUBSTITUTION_SOURCE_NAME_PART,  // {{source_name_part}}
  SUBSTITUTION_SOURCE_FILE_PART,  // {{source_file_part}}
  SUBSTITUTION_SOURCE_DIR,  // {{source_dir}}
  SUBSTITUTION_SOURCE_ROOT_RELATIVE_DIR,  // {{root_relative_dir}}
  SUBSTITUTION_SOURCE_GEN_DIR,  // {{source_gen_dir}}
  SUBSTITUTION_SOURCE_OUT_DIR,  // {{source_out_dir}}

  // Valid for all compiler and linker tools (depends on target).
  SUBSTITUTION_OUTPUT,  // {{output}}
  SUBSTITUTION_ROOT_GEN_DIR,  // {{root_gen_dir}}
  SUBSTITUTION_ROOT_OUT_DIR,  // {{root_out_dir}}
  SUBSTITUTION_TARGET_GEN_DIR,  // {{target_gen_dir}}
  SUBSTITUTION_TARGET_OUT_DIR,  // {{target_out_dir}}
  SUBSTITUTION_TARGET_OUTPUT_NAME,  // {{target_output_name}}

  // Valid for compiler tools.
  SUBSTITUTION_CFLAGS,  // {{cflags}}
  SUBSTITUTION_CFLAGS_C,  // {{cflags_c}}
  SUBSTITUTION_CFLAGS_CC,  // {{cflags_cc}}
  SUBSTITUTION_CFLAGS_OBJC,  // {{cflags_objc}}
  SUBSTITUTION_CFLAGS_OBJCC,  // {{cflags_objcc}}
  SUBSTITUTION_DEFINES,  // {{defines}}
  SUBSTITUTION_INCLUDE_DIRS,  // {{include_dirs}}

  // Valid for linker tools.
  SUBSTITUTION_LINKER_INPUTS,  // {{inputs}}
  SUBSTITUTION_LDFLAGS,  // {{ldflags}}
  SUBSTITUTION_LIBS,  // {{libs}}
  SUBSTITUTION_OUTPUT_EXTENSION,  // {{output_extension}}
  SUBSTITUTION_SOLIBS,  // {{solibs}}

  SUBSTITUTION_NUM_TYPES  // Must be last.
};

// An array of size SUBSTITUTION_NUM_TYPES that lists the names of the
// substitution patterns, including the curly braces. So, for example,
// kSubstitutionNames[SUBSTITUTION_SOURCE] == "{{source}}".
extern const char* kSubstitutionNames[SUBSTITUTION_NUM_TYPES];

// Ninja variables corresponding to each substitution. These do not include
// the dollar sign.
extern const char* kSubstitutionNinjaNames[SUBSTITUTION_NUM_TYPES];

// Returns true if the given substitution pattern references the output
// directory. This is used to check strings that begin with a substitution to
// verify that the produce a file in the output directory.
bool SubstitutionIsInOutputDir(SubstitutionType type);

// Returns true if the given substitution is valid for the named purpose.
bool IsValidSourceSubstitution(SubstitutionType type);
// Both compiler and linker tools.
bool IsValidToolSubstutition(SubstitutionType type);
bool IsValidCompilerSubstitution(SubstitutionType type);
bool IsValidCompilerOutputsSubstitution(SubstitutionType type);
bool IsValidLinkerSubstitution(SubstitutionType type);
bool IsValidLinkerOutputsSubstitution(SubstitutionType type);

// Like the "IsValid..." version above but checks a list of types and sets a
// an error blaming the given source if the test fails.
bool EnsureValidSourcesSubstitutions(
    const std::vector<SubstitutionType>& types,
    const ParseNode* origin,
    Err* err);

#endif  // TOOLS_GN_SUBSTITUTION_TYPE_H_
