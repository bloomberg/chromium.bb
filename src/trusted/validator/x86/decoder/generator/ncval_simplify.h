/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * API to Simplifying instruction set to what is needed for
 * the (x86-64) ncval_reg_sfi validator.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCVAL_SIMPLIFY_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCVAL_SIMPLIFY_H__

struct NaClInstTables;

/* Simplifies the instructions in the instruction tables to match what is
 * needed for the validator.
 */
void NaClNcvalInstSimplify(struct NaClInstTables* inst_tables);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCVAL_SIMPLIFY_H__ */
