/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncdecode_verbose.h - Print routines for validator that are
 * not to be loaded into sel_ldr.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCDECODE_VERBOSE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCDECODE_VERBOSE_H_

#include <stdio.h>
#include "native_client/src/trusted/validator/x86/error_reporter.h"
#include "native_client/src/trusted/validator/types_memory_model.h"

struct NCDecoderInst;
struct Gio;

/*
 * Run the decoder and print out the decoded instructions.
 * Note: Prints to NaClLogGetGio().
 *
 * Parameters are:
 *   mbase - The beging of the memory segment to decode.
 *   vbase - The (virtual) base address of the memory segment.
 *   sz - The number of bytes in the memory segment.
 *   vstate - validator state (or NULL) to use with callbacks.
 */
extern void NCDecodeSegment(uint8_t* mbase, NaClPcAddress vbase,
                            NaClMemorySize sz);

/* Print out the instruction (including the sequence of disassembled
 * hexidecimal bytes) to the given file.
 */
extern void NCPrintInstWithHex(const struct NCDecoderInst* inst,
                               struct Gio* file);

/* Print out the instruction (excluding the sequence of disassembled
 * hexidecimal bytes) to the given file.
 */
extern void NCPrintInstWithoutHex(const struct NCDecoderInst* inst,
                                  struct Gio* file);

/* Generate a (malloc allocated) string describing the instruction (including
 * the sequence of disassembled hexidecimal bytes).
 */
extern char* NCInstWithHexToString(const struct NCDecoderInst* inst);

/* Generate a (malloc allocated) string describing the instruction (excluding
 * the sequence of disassembled hexidecimal bytes).
 */
extern char* NCInstWithoutHexToString(const struct NCDecoderInst* inst);

/* Verbose error reporter for a NCDecoderInst* that reports to
 * NaClLogGetGio().
 */
extern NaClErrorReporter kNCVerboseErrorReporter;

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCDECODE_VERBOSE_H_ */
