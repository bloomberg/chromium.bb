// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/substitution_type.h"

#include <stdlib.h>

#include "tools/gn/err.h"

const char* kSubstitutionNames[SUBSTITUTION_NUM_TYPES] = {
  NULL,  // SUBSTITUTION_LITERAL

  "{{source}}",  // SUBSTITUTION_SOURCE
  "{{source_name_part}}",  // SUBSTITUTION_NAME_PART
  "{{source_file_part}}",  // SUBSTITUTION_FILE_PART
  "{{source_dir}}",  // SUBSTITUTION_SOURCE_DIR
  "{{source_root_relative_dir}}",  // SUBSTITUTION_SOURCE_ROOT_RELATIVE_DIR
  "{{source_gen_dir}}",  // SUBSTITUTION_SOURCE_GEN_DIR
  "{{source_out_dir}}",  // SUBSTITUTION_SOURCE_OUT_DIR

  "{{output}}",  // SUBSTITUTION_OUTPUT
  "{{root_gen_dir}}",  // SUBSTITUTION_ROOT_GEN_DIR
  "{{root_out_dir}}",  // SUBSTITUTION_ROOT_OUT_DIR
  "{{target_gen_dir}}",  // SUBSTITUTION_TARGET_GEN_DIR
  "{{target_out_dir}}",  // SUBSTITUTION_TARGET_OUT_DIR
  "{{target_output_name}}",  // SUBSTITUTION_TARGET_OUTPUT_NAME

  "{{cflags}}",  // SUBSTITUTION_CFLAGS
  "{{cflags_c}}",  // SUBSTITUTION_CFLAGS_C
  "{{cflags_cc}}",  // SUBSTITUTION_CFLAGS_CC
  "{{cflags_objc}}",  // SUBSTITUTION_CFLAGS_OBJC
  "{{cflags_objcc}}",  // SUBSTITUTION_CFLAGS_OBJCC
  "{{defines}}",  // SUBSTITUTION_DEFINES
  "{{include_dirs}}",  // SUBSTITUTION_INCLUDE_DIRS

  "{{inputs}}",  // SUBSTITUTION_LINKER_INPUTS
  "{{ldflags}}",  // SUBSTITUTION_LDFLAGS
  "{{libs}}",  // SUBSTITUTION_LIBS
  "{{output_extension}}",  // SUBSTITUTION_OUTPUT_EXTENSION
  "{{solibs}}",  // SUBSTITUTION_SOLIBS
};

const char* kSubstitutionNinjaNames[SUBSTITUTION_NUM_TYPES] = {
  NULL,  // SUBSTITUTION_LITERAL

  // This isn't written by GN, the name here is referring to the Ninja variable
  // since when we would use this would be for writing source rules.
  "in",  // SUBSTITUTION_SOURCE
  "source_name_part",  // SUBSTITUTION_NAME_PART
  "source_file_part",  // SUBSTITUTION_FILE_PART
  "source_dir",  // SUBSTITUTION_SOURCE_DIR
  "source_root_relative_dir",  // SUBSTITUTION_SOURCE_ROOT_RELATIVE_DIR
  "source_gen_dir",  // SUBSTITUTION_SOURCE_GEN_DIR
  "source_out_dir",  // SUBSTITUTION_SOURCE_OUT_DIR

  "output",  // SUBSTITUTION_OUTPUT
  "root_gen_dir",  // SUBSTITUTION_ROOT_GEN_DIR
  "root_out_dir",  // SUBSTITUTION_ROOT_OUT_DIR
  "target_gen_dir",  // SUBSTITUTION_TARGET_GEN_DIR
  "target_out_dir",  // SUBSTITUTION_TARGET_OUT_DIR
  "target_output_name",  // SUBSTITUTION_TARGET_OUTPUT_NAME

  "cflags",  // SUBSTITUTION_CFLAGS
  "cflags_c",  // SUBSTITUTION_CFLAGS_C
  "cflags_cc",  // SUBSTITUTION_CFLAGS_CC
  "cflags_objc",  // SUBSTITUTION_CFLAGS_OBJC
  "cflags_objcc",  // SUBSTITUTION_CFLAGS_OBJCC
  "defines",  // SUBSTITUTION_DEFINES
  "include_dirs",  // SUBSTITUTION_INCLUDE_DIRS

  "inputs",  // SUBSTITUTION_LINKER_INPUTS
  "ldflags",  // SUBSTITUTION_LDFLAGS
  "libs",  // SUBSTITUTION_LIBS
  "output_extension",  // SUBSTITUTION_OUTPUT_EXTENSION
  "solibs",  // SUBSTITUTION_SOLIBS
};

bool SubstitutionIsInOutputDir(SubstitutionType type) {
  return type == SUBSTITUTION_SOURCE_GEN_DIR ||
         type == SUBSTITUTION_SOURCE_OUT_DIR;
}

bool IsValidSourceSubstitution(SubstitutionType type) {
  return type == SUBSTITUTION_SOURCE ||
         type == SUBSTITUTION_SOURCE_NAME_PART ||
         type == SUBSTITUTION_SOURCE_FILE_PART ||
         type == SUBSTITUTION_SOURCE_DIR ||
         type == SUBSTITUTION_SOURCE_ROOT_RELATIVE_DIR ||
         type == SUBSTITUTION_SOURCE_GEN_DIR ||
         type == SUBSTITUTION_SOURCE_OUT_DIR;
}

bool IsValidToolSubstutition(SubstitutionType type) {
  return type == SUBSTITUTION_OUTPUT ||
         type == SUBSTITUTION_ROOT_GEN_DIR ||
         type == SUBSTITUTION_ROOT_OUT_DIR ||
         type == SUBSTITUTION_TARGET_GEN_DIR ||
         type == SUBSTITUTION_TARGET_OUT_DIR ||
         type == SUBSTITUTION_TARGET_OUTPUT_NAME;
}

bool IsValidCompilerSubstitution(SubstitutionType type) {
  return IsValidToolSubstutition(type) ||
         type == SUBSTITUTION_CFLAGS ||
         type == SUBSTITUTION_CFLAGS_C ||
         type == SUBSTITUTION_CFLAGS_CC ||
         type == SUBSTITUTION_CFLAGS_OBJC ||
         type == SUBSTITUTION_CFLAGS_OBJCC ||
         type == SUBSTITUTION_DEFINES ||
         type == SUBSTITUTION_INCLUDE_DIRS;
}

bool IsValidCompilerOutputsSubstitution(SubstitutionType type) {
  // All tool types except "output" (which would be infinitely recursive).
  return IsValidToolSubstutition(type) &&
         type != SUBSTITUTION_OUTPUT;
}

bool IsValidLinkerSubstitution(SubstitutionType type) {
  return IsValidToolSubstutition(type) ||
         type == SUBSTITUTION_LINKER_INPUTS ||
         type == SUBSTITUTION_LDFLAGS ||
         type == SUBSTITUTION_LIBS ||
         type == SUBSTITUTION_OUTPUT_EXTENSION ||
         type == SUBSTITUTION_SOLIBS;
}

bool IsValidLinkerOutputsSubstitution(SubstitutionType type) {
  // All valid compiler outputs plus the output extension.
  return IsValidCompilerOutputsSubstitution(type) ||
         type == SUBSTITUTION_OUTPUT_EXTENSION;
}

bool EnsureValidSourcesSubstitutions(
    const std::vector<SubstitutionType>& types,
    const ParseNode* origin,
    Err* err) {
  for (size_t i = 0; i < types.size(); i++) {
    if (!IsValidSourceSubstitution(types[i])) {
      *err = Err(origin, "Invalid substitution type.",
          "The substitution " + std::string(kSubstitutionNames[i]) +
          " isn't valid for something\n"
          "operating on a source file such as this.");
      return false;
    }
  }
  return true;
}
