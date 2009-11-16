/*
 * Copyright 2008, Google Inc.
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

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_H_

#include "native_client/src/trusted/validator_x86/types_memory_model.h"

/*
 * ncvalidate.h: exports for ncvalidate.c
 *
 * This is the library interface to the NaCl validator.
 * Basic usage:
 *   rc = NCValidateInit(base, limit, 16)
 *   if rc != 0 fail
 *   for each section:
 *     NCValidateSegment(maddr, vaddr, size);
 *   rc = NCValidateFinish();
 *   if rc != 0 fail
 * Optional reporting routines
 *   Stats_Print()
 *
 * See the README file in this directory for more info on the general
 * structure of the validator.
 */
struct NCValidatorState;

/*
 * NCValidateInit: Initialize NaCl validator internal state
 * Parameters:
 *    vbase: base virtual address for code segment
 *    vlimit: size in bytes of code segment
 *    alignment: 16 or 32, specifying alignment
 * Returns:
 *    an initialized struct NCValidatorState * if everything is okay,
 *    else NULL
 */
struct NCValidatorState *NCValidateInit(const PcAddress vbase,
                                        const PcAddress vlimit,
                                        const uint8_t alignment);

/* Validate a segment */
/* This routine will raise an segmentation exception if you ask
 * it to check memory that can't be accessed. This should of be
 * interpreted as an indication that the module in question is
 * invalid.
 *
 * This routine will not produce verbose validator output, but will
 * print an error message to suggest using ncval for more details on
 * validation errors.
 *
 * NCValidateSegment is used by sel_ldr. For dev/debug purposes,
 * use NCDecodeSegment(). See ncval.c for an example.
 */
void NCValidateSegment(uint8_t *mbase, PcAddress vbase, size_t sz,
                       struct NCValidatorState *vstate);

/* Check targets and alignment. Returns non-zero if there are */
/* safety issues, else returns 1                              */
/* BEWARE: vstate is invalid after this call                  */
int NCValidateFinish(struct NCValidatorState *vstate);

/* BEWARE: this call deallocates vstate.                      */
void NCValidateFreeState(struct NCValidatorState **vstate);

/* Print some interesting statistics... */
void Stats_Print(FILE *f, struct NCValidatorState *vstate);

/* Book-keeping routines called from the decoder. */
void OpcodeHisto(const uint8_t byte1,
                 struct NCValidatorState *vstate);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_H_ */
