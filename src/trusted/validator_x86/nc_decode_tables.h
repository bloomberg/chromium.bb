/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * API for a set of generated decoder tables to use with NaClInstIter.
 * Allows both full and partial parsing, depending on what is defined
 * in the generated decoder tables.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_DECODE_TABLES_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_DECODE_TABLES_H__

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/validator/x86/x86_insts.h"
#include "native_client/src/trusted/validator_x86/nc_decode_tables_types.h"
#include "native_client/src/trusted/validator_x86/gen/ncopcode_prefix.h"

EXTERN_C_BEGIN

struct NaClOp;
struct NaClInst;
struct NaClInstNode;

/* The array used to look up instructions, based on matched prefix selector,
 * the the corresponding (first) opcode byte.
 */
typedef NaClOpcodeArrayOffset
NaclInstTableType[NaClInstPrefixEnumSize][NCDTABLESIZE];

/* Decoder tables used to decode instructions. */
typedef struct NaClDecodeTables {
  /* The table operands. */
  const struct NaClOp* operands_table;
  const struct NaClInst* opcodes_table;
  /* The definition of the undefined instruction. */
  const struct NaClInst* undefined;
  /* The set of look up tables for opcode instructions, based
   * on the instruction prefix, and the corresponding (first) opcode byte.
   */
  const NaclInstTableType* inst_table;
  /* The look up table that returns the corresponding prefix mask
   * for the byte value, or zero if the byte doesn't define a valid
   * prefix byte.
   */
  const uint32_t* prefix_mask;
  /* The trie of hard coded instructions. */
  const struct NaClInstNode* hard_coded;
} NaClDecodeTables;

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_DECODE_TABLES_H__ */
