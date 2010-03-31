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

#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_read_segment.h"
#include "native_client/src/trusted/validator_x86/ncval_driver.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"
#include "native_client/src/trusted/validator_x86/ncvalidator_registry.h"

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
    if (GrokCstringFlag("-hex_text", arg, &FLAGS_hex_text) ||
        GrokBoolFlag("-trace", arg, &NACL_FLAGS_validator_trace) ||
        GrokBoolFlag("-trace_verbose",
                     arg, &NACL_FLAGS_validator_trace_verbose)) {
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
#define NACL_MAX_INPUT_LINE 4096

typedef struct NaClValidatorByteArray {
  uint8_t bytes[NACL_MAX_INPUT_LINE];
  NaClPcAddress base;
  NaClMemorySize num_bytes;
} NaClValidatorByteArray;

static struct GioFile gio_err_stream;

static void Fatal(const char* format, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);

static void Fatal(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  gprintf((struct Gio*) &gio_err_stream, "Fatal: ");
  gvprintf((struct Gio*) &gio_err_stream, format, ap);
  va_end(ap);
  /* always succed, so that the testing framework works. */
  exit(0);
}

static int ValidateLoad(int argc, const char* argv[],
                        NaClValidatorByteArray* data) {
  argc = GrokFlags(argc, argv);
  if (argc != 1) {
    Fatal("expected: %s <options>\n", argv[0]);
  }
  if (0 == strcmp(FLAGS_hex_text, "")) {
    data->num_bytes = (NaClMemorySize)
        NaClReadHexTextWithPc(stdin, &data->base, data->bytes,
                              NACL_MAX_INPUT_LINE);
  } else {
    FILE* input = fopen(FLAGS_hex_text, "r");
    if (NULL == input) {
      Fatal("Can't open hex text file: %s\n", FLAGS_hex_text);
    }
    data->num_bytes = (NaClMemorySize)
        NaClReadHexTextWithPc(input, &data->base, data->bytes,
                              NACL_MAX_INPUT_LINE);
    fclose(input);
  }
  return argc;
}

int main(int argc, const char* argv[]) {
  NaClValidatorByteArray data;
  GioFileRefCtor(&gio_err_stream, stdout);
  NaClLogDisableTimestamp();
  argc = ValidateLoad(argc, argv, &data);
  if (NaClRunValidatorBytes(argc, argv, (uint8_t*) &data.bytes,
                            data.num_bytes,
                            data.base)) {
    printf("***module is safe***\n");
  } else {
    printf("***MODULE IS UNSAFE***\n");
  }
  /* always succeed, so that the testing framework works. */
  return 0;
}
