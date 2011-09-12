/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * API to compressing tables of modeled instructions.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NC_COMPRESS_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NC_COMPRESS_H__

#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_decode_tables.h"

/* Define the maximum number of instruction operands (over
 * all instructions) we will allow.  Note: before compression,
 * there should a little less than 3000 operands. Hence,
 * 10000 is a (very) safe value to use as a limit, and should
 * not need to be changed. Also, the table generator will
 * complain (and then fail) if this number is exceeded. If that
 * happens, update this constant as is appropriate.
 */
#define NACL_MAX_OPERANDS_TOTAL 10000

/* Define the maximum number of instructions we will allow.
 * Note: before compression there should be less than 3000
 * instructions. Hence, 10000 is a (very) safe value to use
 * as a limit, and should not need to be changed. Also, the
 * table generator will complain (and then fail) if this number
 * is exceeded. If that happens, update this constant as
 * is appropriate.
 */
#define NACL_MAX_INSTRUCTIONS_TOTAL 10000

/* Define the maximum number of prefix/opcode entries that
 * we will allow. Note: before compression there are
 * 256 * NaClInstPrefixEnumSize(19) = 4864 entries.
 * Hence, 100000 is a (very) safe value to use as a limit,
 * and should not need to be changed. Also, the
 * table generator will complain (and then fail) if this number
 * is exceeded. If that happens, update this constant as
 * is appropriate.
 */
#define NACL_MAX_PREFIX_OPCODE_ENTRIES 10000

/* The type for the array used to look up instructions, based on matched
 * prefix selector and the corresponding (first) opcode byte.
 */
typedef NaClModeledInst*
NaclModeledInstTableType[NCDTABLESIZE][NaClInstPrefixEnumSize];

/* The type for the array of compressed instructions. */
typedef NaClModeledInst*
NaclCompressedTableType[NACL_MAX_INSTRUCTIONS_TOTAL];

/* The type for the array of opcode lookup entries. */
typedef NaClPrefixOpcodeArrayOffset
NaClCompressedOpcodeLookupType[NACL_MAX_PREFIX_OPCODE_ENTRIES];

/* Models the arrays used in table generation, and the corresponding
 * tables used to model the compressed data.
 */
typedef struct NaClInstTables {
  /* The lookup table of posssible instructions, based on opcode and
   * prefix selector.
   */
  NaClModeledInst* inst_table[NCDTABLESIZE][NaClInstPrefixEnumSize];
  /* The root of the hard coded instructions. */
  NaClModeledInstNode* inst_node_root;
  /* The array of modeled set of operands defined for instructions. */
  NaClOp operands[NACL_MAX_OPERANDS_TOTAL];
  /* The number of operands in nacl_operands. */
  size_t operands_size;
  /* The instruction that models an undefined instruction. */
  NaClModeledInst* undefined_inst;
  /* The generated array of compressed operands. */
  NaClOp ops_compressed[NACL_MAX_OPERANDS_TOTAL];
  /* The number of compressed operands added to ops_compressed. */
  size_t ops_compressed_size;
  /* The generated array of compressed instructions. */
  NaClModeledInst* inst_compressed[NACL_MAX_INSTRUCTIONS_TOTAL];
  /* The number of compressed instructions added to inst_compressed. */
  size_t inst_compressed_size;
  /* The generated table of opcode lookup indicies. */
  NaClOpcodeArrayOffset opcode_lookup[NACL_MAX_PREFIX_OPCODE_ENTRIES];
  /* The size of array opcode_lookup. */
  size_t opcode_lookup_size;
  /* The generated table of entry points into opcode_lookup, for each
   * possible prefix.
   */
  NaClPrefixOpcodeArrayOffset opcode_lookup_entry[NaClInstPrefixEnumSize];
  /* The first non-null opcode value stored in the generated
   * table opcode_lookup, for each possible prefix.
   */
  uint8_t opcode_lookup_first[NaClInstPrefixEnumSize];
  /* The last non-null opcode vlaue stored in the generated
   * table opcode_lookupk, for each possible prefix.
   */
  uint8_t opcode_lookup_last[NaClInstPrefixEnumSize];
} NaClInstTables;

/* Given the tables passed in, compresses instructions, operands,
 * and prefix/opcode lookup tables.
 */
void NaClOpCompress(NaClInstTables* inst_tables);

/* Given the tables passed in, return the index (in the compressed instruction
 * array) associated with the given instruction.
 */
NaClOpcodeArrayOffset NaClFindInstIndex(NaClInstTables* inst_tables,
                                        const NaClModeledInst* inst);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NC_COMPRESS_H__ */
