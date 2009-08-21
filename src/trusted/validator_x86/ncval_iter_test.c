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
                                 (uint8_t) FLAGS_alignment, RegR15, stdout);
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
