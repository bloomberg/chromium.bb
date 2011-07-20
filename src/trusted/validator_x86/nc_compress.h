/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * API to compressing tables of modeled instructions.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_COMPRESS_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_COMPRESS_H__

#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

/* The type for the array used to look up instructions, based on matched
 * prefix selector and the corresponding (first) opcode byte.
 */
typedef NaClModeledInst*
nacl_inst_table_type[NCDTABLESIZE][NaClInstPrefixEnumSize];

/* The type for the array of compressed instructions. */
typedef NaClModeledInst*
nacl_compressed_table_type[NACL_MAX_INSTRUCTIONS_TOTAL];

/* Models the arrays used in table generation, and the corresponding
 * tables used to model the compressed data.
 */
typedef struct NaClInstTables {
  /* The lookup table of posssible instructions, based on opcode and
   * prefix selector.
   */
  nacl_inst_table_type* inst_table;
  /* The root of the hard coded instructions. */
  NaClModeledInstNode** inst_node_root;
  /* The generated array of compressed instructions. */
  nacl_compressed_table_type* inst_compressed;
  /* The number of compressed instructions added to inst_compressed. */
  size_t* inst_compressed_size;
  /* The array of modeled set of operands defined for instructions. */
  NaClOp* operands;
  /* The number of operands in nacl_operands. */
  size_t* operands_size;
  /* The generated array of compressed operands. */
  NaClOp* ops_compressed;
  /* The number of compressed operands added to ops_compressed. */
  size_t* ops_compressed_size;
  /* The instruction that models an undefined instruction. */
  NaClModeledInst** undefined_inst;
} NaClInstTables;

/* Given the tables passed in, compresses instructions and operands. */
void NaClOpCompress(NaClInstTables* inst_tables);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_COMPRESS_H__ */
