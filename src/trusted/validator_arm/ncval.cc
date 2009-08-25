/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ncval.c - command line validator for NaCl (ARM).
 * Mostly for testing.
 */

#include <stdio.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <fstream>

#include "native_client/src/shared/utils/flags.h"

#include "native_client/src/trusted/validator_arm/arm_validate.h"
#include "native_client/src/trusted/validator_arm/stack_adjust_patterns.h"
#include "native_client/src/trusted/validator_arm/branch_patterns.h"
#include "native_client/src/trusted/validator_arm/segment_parser.h"
#include "native_client/src/trusted/validator_arm/ncdecode.h"
#include "native_client/src/trusted/validator_arm/nop_patterns.h"

// True iff stack adjustment patterns should be applied.
static Bool FLAGS_stack_adjustments = TRUE;

/*
 * Flag defining an input file to use to generate a code segment
 * to decode. "-' denotes standard input should be used to
 * parse the code segment to decode.
 */
static char* FLAGS_decode_segment = const_cast<char*>("");

/*
 * When true, set executable name to argv[0].
 */
static Bool FLAGS_set_executable_name = TRUE;

// Process command line arguments to remove options from
// argv. Return the new argc value after the options have
// been removed (and processed).
static int GrokFlags(int argc, const char *argv[]) {
  int i;
  int new_argc = (argc == 0 ? 0 : 1);
  for (i = 1; i < argc; ++i) {
    if (GrokBoolFlag("--branch_in_text_segment", argv[i],
                     &FLAGS_branch_in_text_segment) ||
        GrokBoolFlag("--branch_word_boundary", argv[i],
                     &FLAGS_branch_word_boundary) ||
        GrokBoolFlag("--branch_into_constant_pool", argv[i],
                     &FLAGS_branch_into_constant_pool) ||
        GrokBoolFlag("--report_disallowed", argv[i],
                     &FLAGS_report_disallowed) ||
        GrokIntFlag("--code_block_size", argv[i],
                    &FLAGS_code_block_size) ||
        GrokBoolFlag("--stack_adjustments", argv[i],
                     &FLAGS_stack_adjustments) ||
        GrokBoolFlag("--set_executable_name", argv[i],
                     &FLAGS_set_executable_name) ||
        GrokBoolFlag("--count_pattern_usage", argv[i],
                     &FLAGS_count_pattern_usage) ||
        GrokCstringFlag("--decode_segment", argv[i],
                        &FLAGS_decode_segment) ||
        GrokBoolFlag("--show_counted_instructions", argv[i],
                     &FLAGS_show_counted_instructions)) {
    } else {
      // Default to not a flag.
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}


// process the command line arguments.
static const char* GrokArgv(int argc, const char *argv[]) {
  if (FLAGS_set_executable_name) {
    SetValidateProgramName(argv[0]);
  }

  if (2 == argc) {
    return argv[1];
  } else if (0 != strcmp(FLAGS_decode_segment, "") && 1 == argc) {
    return "";
  } else if (argc > 1) {
    ValidateFatal("command line argument %s not recognized.\n\t"
                  "expected: '%s [options] executable'",
                  argv[1],
                  GetValidateProgramName());
  } else {
    ValidateFatal("\n\texpected: '%s [options] executable'",
                  GetValidateProgramName());
  }

  return 0;
}

int main(int argc, const char *argv[]) {
  const char *loadname = GrokArgv(GrokFlags(argc, argv), argv);

  InstallBranchPatterns();

  if (FLAGS_stack_adjustments) {
    InstallStackAdjustPatterns();
  }

  if (FLAGS_count_pattern_usage) {
    InstallInstructionCounterPatterns();
  }

  if (0 == strcmp(FLAGS_decode_segment, "")) {
    if (NULL != loadname) {
      fprintf(stdout, "load: %s\n", loadname);

      ncfile* ncf = nc_loadfile(loadname);
      if (NULL == ncf) {
        ValidateFatal("nc_loadfile(%s): %s\n", strerror(errno));
      }

      ValidateNcFile(ncf, loadname);

      nc_freefile(ncf);
    }
  } else {
    std::istream* input = NULL;
    if (strcmp(FLAGS_decode_segment, "-") == 0) {
      input = &std::cin;
    } else {
      input = new std::ifstream(FLAGS_decode_segment);
      if (!input) {
        fprintf(stdout, "Can't open code segment: %s", FLAGS_decode_segment);
        delete input;
        input = NULL;
      }
    }
    if (NULL != input) {
      SegmentParser segment_parser(input);
      CodeSegment code_segment;
      segment_parser.Initialize(&code_segment);
      ValidateCodeSegment(&code_segment);
      if (input != &std::cin) {
        delete input;
      }
    }
  }

  if (FLAGS_count_pattern_usage) {
    PrintPatternUsageCounts();
  }

  int exit_code = ValidateExitCode();
  if (0 != exit_code) {
    fprintf(stdout, "Errors found. Exiting with exit code: %d\n", exit_code);
  }
  return exit_code;
}
