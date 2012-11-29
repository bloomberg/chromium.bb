/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCVALIDATE_DETAILED_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCVALIDATE_DETAILED_H__

/*
 * ncvalidate_details.h: Secondary API for the validator to the segment-based
 * sandbox.
 *
 * This is a secondary interface to the validator for the segment-based sandbox.
 * This version should be used when details about reporting errors is needed.
 * In particular, this interface should be used when the validator is run
 * in verbose or stubout mode. In these cases, getting details right
 * (i.e. the instruction that causes a code segment to violate NaCl rules)
 * is important. For verbose mode, this implies that the
 * error messages will report each instruction that violates a NaCl rule. For
 * stubout mode, it will automatically stub out (i.e. replace with HALT
 * instructions) instructions that violate NaCl rules.
 *
 * See ncvalidate.h for the primary interface to the segment-based sandbox
 * NaCl validator.
 *
 * This secondary interface is considerbly slower than the primary interface
 * in that it does 2 walks over the code segment instead of one. However, by
 * doing this second walk, it can generate more detailed error reports.
 * The secondary interface is engaged if one calls NCValidateInitDetailed
 * in place of NCValidateInit. The rest of the interface to the
 * NaCl validator is the same.
 *
 * Basic usage:
 *   if (!NaClArchSuppported()) fail
 *   vstate = NCValidateInitDetailed(base, size, cpu_features)
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

#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate.h"

/*
 * NCValidateInitDetailed: Initialize NaCl validator internal state. Focus
 * is on error details rather then performance. Note: The paramters and
 * return values for this function is the same as NCValidateInit
 * from the primary interface to the NaCl validator, and replaces it
 * in the secondary interface.o
 *
 * Parameters:
 *    vbase: base virtual address for code segment
 *    codesize: size in bytes of code segment
 *    features: the features supported by the CPU that will run the code
 * Returns:
 *    an initialized struct NCValidatorState * if everything is okay,
 *    else NULL
 */
struct NCValidatorState *NCValidateInitDetailed(
    const NaClPcAddress vbase,
    const NaClMemorySize codesize,
    const NaClCPUFeaturesX86 *features);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCVALIDATE_DETAILED_H__ */
