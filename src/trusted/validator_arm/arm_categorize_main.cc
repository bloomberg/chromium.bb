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

#include <stdio.h>
#include <string.h>

#include "native_client/src/trusted/validator_arm/types_cpp.h"
#include "native_client/src/trusted/validator_arm/arm_inst_modeling.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/trusted/validator_arm/arm_cat_compress.h"
#include "native_client/src/trusted/validator_arm/arm_compress.h"
#include "native_client/src/trusted/validator_arm/decoder_generator.h"

#define DEBUGGING 0
#include "native_client/src/trusted/validator_arm/debugging.h"

/* When non-negative, specifies the number of instruction patterns
 * to install into the instruction trie. Used as a debugging tool
 * that allows testing on small tries.
 */
extern int FLAGS_patterns;

/*
 * Holds the command-line specification of which nodes in the trie that
 * should be printed.
 */
static PrintArmInstructionForm FLAGS_print_form = kPrintArmInstructionFormError;

/*
 * Holds whether we should validate the model using the generated instruction
 * trie.
 */
static Bool FLAGS_validate = FALSE;

/*
 * Defines whether we should generate source code to parse ARM instructions.
 */
static char* FLAGS_generate_parser = NULL;

/*
 * Defines whether we should print the instruction trie after compressing.
 */
static Bool FLAGS_print_compressed = FALSE;

/*
 * Process command line arguments to remove options from
 * argv. Return the new argc value after the options have
 * been removed (and processed).
 */
static int GrockFlags(int argc, const char *argv[]) {
  int i;
  int new_argc = 1;
  for (i = 1; i < argc; ++i) {
    DEBUG(printf("consider %d = '%s'\n", i, argv[i]));
    if (strcmp("--all", argv[i]) == 0) {
      FLAGS_print_form = kPrintArmInstructionFormAll;
    } else if (strcmp("--leaf", argv[i]) == 0) {
      FLAGS_print_form = kPrintArmInstructionFormLeaf;
    } else if (strcmp("--error", argv[i]) == 0) {
      FLAGS_print_form = kPrintArmInstructionFormError;
    } else if (!(GrokBoolFlag("--validate", argv[i],
                              &FLAGS_validate) ||
                 GrokCstringFlag("--generate_parser", argv[i],
                                 &FLAGS_generate_parser) ||
                 GrokIntFlag("--patterns", argv[i],
                             &FLAGS_patterns) ||
                 GrokBoolFlag("--print_compressed", argv[i],
                              &FLAGS_print_compressed))) {
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}

/*
 * process the command line arguments.
 */
static void GrockArgv(int argc, const char *argv[]) {
  if (1 < argc) {
    int i;
    fprintf(stderr, "usage: arm_categorize [options]\n");
    for (i = 1; i < argc; ++i) {
      fprintf(stderr, "ignoring argument: %s\n", argv[i]);
    }
  }
}

/*
 * Read in instructions and build the corresopnding trie
 * of bit patterns. Validate properties expected for the
 * bit patterns when possible.
 */
int main(int argc, const char* argv[]) {
  GrockArgv(GrockFlags(GrokArmBuildFlags(argc, argv), argv), argv);

  BuildArmInstructionSet();

  ArmInstructionTrieData trie_data;
  InitializeArmInstructionTrieData(&trie_data);

  BuildArmInstructionTrie(&trie_data, !FLAGS_validate);

  if (FLAGS_validate) {
    ValidateArmDecisionTrie(&trie_data);
    int number_visited_leaves =
        PrintArmInstructionTrie(&trie_data, FLAGS_print_form);
    printf("*Note*: %d uniquely categorized patterns\n", number_visited_leaves);
  } else if (FLAGS_print_compressed) {
    PrintArmInstructionTrie(&trie_data, kPrintArmInstructionFormAll);
  }

  if (NULL != FLAGS_generate_parser) {
    if (FLAGS_validate) {
      CompressedArmInstructionTrie compressed_trie(&trie_data);
      compressed_trie.Compress();
      printf("*Note*: compressed %d uniquely categorized patterns\n",
             int(compressed_trie.Size()));
      if (FLAGS_print_compressed) {
        PrintArmInstructionTrie(&trie_data, kPrintArmInstructionFormAll);
      }
    }

    // Open file to store generated results.
    FILE* file;
    if (strcmp(FLAGS_generate_parser, "-") == 0) {
      file = stdout;
    } else {
      file = fopen(FLAGS_generate_parser, "w");
      if (NULL == file) {
        printf("*Error* can't open file: '%s'\n", FLAGS_generate_parser);
        return 0;
      }
      printf("Generating '%s'...\n", FLAGS_generate_parser);
    }
    DecoderGenerator generator(&trie_data, file);
    generator.Generate();
    if (file != stdout) fclose(file);
  }

  return 0;
}
