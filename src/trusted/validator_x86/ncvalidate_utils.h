/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* Some useful utilities for validator patterns. */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_UTILS_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_UTILS_H__

#include "native_client/src/trusted/validator_x86/ncopcode_desc.h"

struct NcInstState;
struct ExprNodeVector;

/* Special flag set to find set/use of an operand. */
extern const OperandFlags NcOpSetOrUse;

/* Returns true if the instruction is a binary operation with
 * the given mnemonic name, and whose arguments are the given registers.
 *
 * Parameters:
 *   opcode - The opcode corresponding to the instruction to check.
 *   name - The mnemonic name of the instruction.
 *   vector - The expression vector corresponding to the instruction to check.
 *   reg_1 - The register expected as the first argument
 *   reg_2 - The register expected as the second argument.
 */
Bool NcIsBinaryUsingRegisters(struct Opcode* opcode,
                              InstMnemonic name,
                              struct ExprNodeVector* vector,
                              OperandKind reg_1,
                              OperandKind reg_2);

/* Returns true if the instruction corresponds to a binary operation whose
 * result is put into REG_SET, and the resulst is computed using the values in
 * REG_SET and REG_USE.
 *
 * Parameters:
 *   opcode - The opcode corresponding to the instruction to check.
 *   name - The mnemonic name of the binary operation.
 *   vector - The expression vector corresponding to the instruction to check.
 *   reg_set - The register set by the binary operation.
 *   reg_use - The register whose value is used (along with reg_set) to generate
 *             the or value.
 */
Bool NcIsBinarySetUsingRegisters(struct Opcode* opcode,
                                 InstMnemonic name,
                                 struct ExprNodeVector* vector,
                                 OperandKind reg_1,
                                 OperandKind reg_2);

/* Returns true if the instruction corresponds to a move from
 * REG_USE to REG_SET.
 *
 * Parameters:
 *   opcode - The opcode corresponding to the instruction to check.
 *   vector - The expression vector corresponding to the instruction to check.
 *   reg_set - The register set by the move.
 *   reg_use - The register whose value is used to define the set.
 */
Bool NcIsMovUsingRegisters(struct Opcode* opcode,
                           struct ExprNodeVector* vector,
                           OperandKind reg_set,
                           OperandKind reg_use);

/* Returns true if the given instruction's first operand corresponds to
 * a set of the register with the given name.
 *
 * Parameters:
 *   inst - The instruction to check.
 *   reg_name - The name of the register to check if set.
 */
Bool NcOperandOneIsRegisterSet(struct NcInstState* inst,
                               OperandKind reg_name);

/* Returns true if the given instruction's first operand corresponds to
 * a 32-bit value that is zero extended.
 *
 * Parameters:
 *   inst - The instruction to check.
 */
Bool NcOperandOneZeroExtends(struct NcInstState* inst);

/* Returns true if the given instruction is binary where the first
 * Operand is a register set on the given register, and the
 * second operand corresponds to a 32-bit vlaue that is zero extended.
 */
Bool NcAssignsRegisterWithZeroExtends(struct NcInstState* inst,
                                      OperandKind reg_name);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_UTILS_H__ */
