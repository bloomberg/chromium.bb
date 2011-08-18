/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncvalidate_iter.c
 * Validate x86 instructions for Native Client
 *
 */

#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter.h"

#include <assert.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/x86/decoder/ncop_exps.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_iter.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator/x86/halt_trim.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter_internal.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidator_registry.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/nc_jumps.h"
#include "native_client/src/trusted/validator/x86/nc_segment.h"
#include "native_client/src/trusted/validator_x86/ncdis_decode_tables.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

void NaClValidatorStateSetErrorReporter(NaClValidatorState *state,
                                        NaClErrorReporter *reporter) {
  switch (reporter->supported_reporter) {
    case NaClNullErrorReporter:
    case NaClInstStateErrorReporter:
      state->error_reporter = reporter;
      return;
    default:
      break;
  }
  reporter->printf(
      reporter,
      "*** FATAL: using unsupported error reporter! ***\n",
      "*** NaClInstStateErrorReporter expected but found %s ***\n",
      NaClErrorReporterSupportedName(reporter->supported_reporter));
  exit(1);
}

static void NaClVerboseErrorPrintInst(NaClErrorReporter* self,
                                      struct NaClInstState* inst) {
  NaClInstStateInstPrint(NaClLogGetGio(), inst);
}

NaClErrorReporter kNaClVerboseErrorReporter = {
  NaClInstStateErrorReporter,
  NaClVerboseErrorPrintf,
  NaClVerboseErrorPrintfV,
  (NaClPrintInst) NaClVerboseErrorPrintInst
};
