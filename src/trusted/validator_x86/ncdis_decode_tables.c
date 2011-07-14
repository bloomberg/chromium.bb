/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_x86/nc_decode_tables.h"

#include "native_client/src/trusted/validator_x86/ncopcode_desc.h"
#if NACL_TARGET_SUBARCH == 64
# include "native_client/src/trusted/validator_x86/gen/nc_opcode_table_64.h"
#else
# include "native_client/src/trusted/validator_x86/gen/nc_opcode_table_32.h"
#endif

static const NaClDecodeTables kDecoderTables = {
  &g_Undefined_Opcode,
  (const nacl_inst_table_type*)(&g_OpcodeTable),
  kNaClPrefixTable,
  g_OpcodeSeq
};

const struct NaClDecodeTables* kNaClDecoderTables = &kDecoderTables;

void NaClChangeOpcodesToXedsModel() {
  /* Changes opcodes to match xed. That is change:
   * 0f0f..1c: Pf2iw $Pq, $Qq => 0f0f..2c: Pf2iw $Pq, $Qq
   * 0f0f..1d: Pf2id $Pq, $Qq => 0f0f..2d: Pf2id $Pq, $Qq
   */
  g_OpcodeTable[Prefix0F0F][0x2c] = g_OpcodeTable[Prefix0F0F][0x1c];
  g_OpcodeTable[Prefix0F0F][0x1c] = NULL;
  g_OpcodeTable[Prefix0F0F][0x2d] = g_OpcodeTable[Prefix0F0F][0x1d];
  g_OpcodeTable[Prefix0F0F][0x1d] = NULL;
}
