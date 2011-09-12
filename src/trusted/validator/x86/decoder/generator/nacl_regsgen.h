/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NACL_REGSREG_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NACL_REGSREG_H_

/*
 * Generates file nc_subregs.h, which contains the the following:
 *
 * static int NaClGpSubregIndex[NaClOpKindEnumSize];
 *
 *    Contains mapping from nacl operand kind, to the
 *    the corresponding general purpose 64 bit register index
 *    that it is a subpart of.
 *    That is, the index into NaClRegTable64, or NACL_REGISTER_UNDEFINED
 *    if not a general purpose register.
 *
 * static int NaClGpReg64Index[NaClOpKindEnumSize];
 *
 *    Contains the mapping from nacl operand kind, to the
 *    corresponding general purpose 64 bit register index.
 *    That is, the index into NaClRegTable64, or NACL_REGISTERED_UNDEFIND
 *    if not a general purpose 64 bit register.
 */

struct Gio;

/* Build the arrays NaClGpSubregIndex and NaClGpReg64Index into
 * the file defined by the argument, for the 32-bit architecture
 */
void NaClPrintGpRegisterIndexes_32(struct Gio* f);

/* Build the arrays NaClGpSubregIndex and NaClGpReg64Index into
 * the file defined by the argument, for the 64-bit architecture
 */
void NaClPrintGpRegisterIndexes_64(struct Gio* f);


#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NACL_REGSREG_H_ */
