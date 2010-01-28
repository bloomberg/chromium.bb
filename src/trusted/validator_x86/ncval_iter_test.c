/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Run the validator on a (hex) text segment to do a unit test.
 */

#include <stdio.h>
#include <string.h>

#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_read_segment.h"
#include "native_client/src/trusted/validator_x86/ncval_driver.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"

/* Flag defining the aliagnment to use. */
static int FLAGS_alignment = 32;

/* Flag defining the name of a hex text to be used as the code segment. Assumes
 * that the pc associated with the code segment is defined by
 * FLAGS_decode_pc.
 */
static char* FLAGS_hex_text = "";

/* Recognizes flags in argv, processes them, and then removes them.
 * Returns the updated value for argc.
 */
static int GrokFlags(int argc, const char* argv[]) {
  int i;
  int new_argc;
  if (argc == 0) return 0;
  new_argc = 1;
  for (i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (GrokIntFlag("-alignment", arg, &FLAGS_alignment) ||
        GrokCstringFlag("-hex_text", arg, &FLAGS_hex_text)) {
      continue;
    } else {
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}

/* Defines the maximum number of characters allowed on an input line
 * of the input text defined by the commands command line option.
 */
#define MAX_INPUT_LINE 4096

typedef struct ValidateData {
  uint8_t bytes[MAX_INPUT_LINE];
  PcAddress base;
  MemorySize num_bytes;
} ValidateData;

static void* ValidateLoad(int argc, const char* argv[]) {
  ValidateData* data;
  argc = GrokFlags(argc, argv);
  if (argc != 1) {
    NcValidatorMessage(LOG_FATAL, NULL, "expected: %s <options>\n", argv[0]);
  }
  data = (ValidateData*) malloc(sizeof(ValidateData));
  if (NULL == data) {
    NcValidatorMessage(LOG_FATAL, NULL, "Unsufficient memory to run");
  }
  if (0 == strcmp(FLAGS_hex_text, "")) {
    data->num_bytes = (MemorySize)
        NcReadHexTextWithPc(stdin, &data->base, data->bytes, MAX_INPUT_LINE);
  } else {
    FILE* input = fopen(FLAGS_hex_text, "r");
    if (NULL == input) {
      NcValidatorMessage(LOG_FATAL, NULL,
                         "Can't open hex text file: %s\n", FLAGS_hex_text);
    }
    data->num_bytes = (MemorySize)
        NcReadHexTextWithPc(input, &data->base, data->bytes, MAX_INPUT_LINE);
    fclose(input);
  }
  return data;
}

static int ValidateAnalyze(ValidateData* data) {
  NcValidatorState* state;
  state = NcValidatorStateCreate(data->base, data->num_bytes,
                                 (uint8_t) FLAGS_alignment, RegR15, FALSE,
                                 stdout);
  if (NULL == state) {
    NcValidatorMessage(LOG_FATAL, NULL, "Unable to create validator state");
  }
  NcValidateSegment(data->bytes, data->base, data->num_bytes, state);
  NcValidatorStatePrintStats(stdout, state);
  if (NcValidatesOk(state)) {
    NcValidatorMessage(LOG_INFO, state, "***module is safe***\n");
  } else {
    NcValidatorMessage(LOG_INFO, state, "***MODULE IS UNSAFE***\n");
  }
  NcValidatorStateDestroy(state);
  return 0;
}

int main(int argc, const char* argv[]) {
  NaClLogDisableTimestamp();
  return NcRunValidator(argc, argv,
                        (NcValidateLoad) ValidateLoad,
                        (NcValidateAnalyze) ValidateAnalyze);
}
