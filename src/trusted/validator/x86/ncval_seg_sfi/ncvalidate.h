/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCVALIDATE_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCVALIDATE_H__

#include <stdio.h>
#include "native_client/src/trusted/cpu_features/arch/x86/cpu_x86.h"
#include "native_client/src/trusted/validator/types_memory_model.h"

/*
 * ncvalidate.h: Validator for the segment-based sandbox.
 *
 * This is the primary library interface to the validator for the
 * segment-based sandbox. This version should be used when performance
 * is important. See ncvalidate_detailed.h for a secondary API which
 * provides more details when reporting errors.
 *
 * Basic usage:
 *   if (!NaClArchSuppported()) fail
 *   vstate = NCValidateInit(base, size, features);
 *   if vstate == 0 fail
 *   for each section:
 *     NCValidateSegment(maddr, base, size, vstate);
 *   rc = NCValidateFinish();
 *   if rc != 0 fail
 *   NCValidateFreeState(&vstate);
 *
 * See the README file in this directory for more info on the general
 * structure of the validator.
 */
struct Gio;
struct NCDecoderInst;
struct NCValidatorState;
struct NaClErrorReporter;

/*
 * Set the maximum number of diagnostic errors to be reported to the
 * given value (-1 implies all error messages).
 */
void NCValidateSetNumDiagnostics(struct NCValidatorState *vstate,
                                 int num_diagnostics);

/*
 * NCValidateInit: Initialize NaCl validator internal state.
 * Parameters:
 *    vbase: base virtual address for code segment
 *    codesize: size in bytes of code segment
 *    features: the features supported by the CPU that will run the code
 * Returns:
 *    an initialized struct NCValidatorState * if everything is okay,
 *    else NULL
 */
struct NCValidatorState *NCValidateInit(const NaClPcAddress vbase,
                                        const NaClMemorySize codesize,
                                        const int readonly_text,
                                        const NaClCPUFeaturesX86 *features);

/*
 * Allows "stub out mode" to be enabled, in which some unsafe
 * instructions will be rendered safe by replacing them with HLT
 * instructions.
 */
void NCValidateSetStubOutMode(struct NCValidatorState *vstate,
                              int do_stub_out);

/*
 * Set the maximum number of diagnostic errors to be reported to the
 * given value (-1 implies all error messages).
 */
void NCValidateSetNumDiagnostics(struct NCValidatorState* vstate,
                                 int num_diagnostics);

/* Changes the error reporter to the given error reporter
 * for the given validator state.
 */
void NCValidateSetErrorReporter(struct NCValidatorState* vstate,
                                struct NaClErrorReporter* error_reporter);

/* Validate a segment */
/* This routine will raise an segmentation exception if you ask
 * it to check memory that can't be accessed. This should of be
 * interpreted as an indication that the module in question is
 * invalid.
 */
void NCValidateSegment(uint8_t *mbase, NaClPcAddress vbase,
                       NaClMemorySize sz,
                       struct NCValidatorState *vstate);

/* Validate a segment for dynamic code replacement */
/* This routine checks that the code found at mbase_old
 * can be dynamically replaced with the code at mbase_new
 * safely. Returns non-zero if successful.
 */
int NCValidateSegmentPair(uint8_t *mbase_old, uint8_t *mbase_new,
                          NaClPcAddress vbase, size_t sz,
                          const NaClCPUFeaturesX86 *features);

/* Check targets and alignment. Returns non-zero if there are */
/* safety issues, else returns 1                              */
/* BEWARE: vstate is invalid after this call                  */
int NCValidateFinish(struct NCValidatorState *vstate);

/* BEWARE: this call deallocates vstate.                      */
void NCValidateFreeState(struct NCValidatorState **vstate);

/* Print some interesting statistics... (optional). If used,
 * should be called between NCValidateFinish and
 * NCValidateFreeState.
 *
 * Note: Uses error reporter of validator to print messages.
 * The default error reporter of the validator will not
 * print any messages. To actually get the messages, you
 * must associate an error reporter with the validator using
 * NCValidateSetErrorReporter.
 */
void NCStatsPrint(struct NCValidatorState *vstate);

/* Returns the default value used for controlling printing
 * of validator messages.
 * If zero, no messages are printed.
 * If >0, only that many diagnostic errors are printed.
 * If negative, all validator diagnostics are printed.
 */
int NCValidatorGetMaxDiagnostics(void);

/* Changes default flag for printing validator error messages.
 * If zero, no messages are printed.
 * If >0, only that many diagnostic errors are printed.
 * If negative, all validator diagnostics are printed.
 */
void NCValidatorSetMaxDiagnostics(int new_value);

/* Returns 1 if any code has been overwritten with halts. */
int NCValidatorDidStubOut(struct NCValidatorState *vstate);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCVALIDATE_H__ */
