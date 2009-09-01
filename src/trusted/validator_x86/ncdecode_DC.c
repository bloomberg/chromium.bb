/*
 * Copyright 2009, Google Inc.
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

/*
 * Defines two byte opcodes beginning with DC
 */

#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

void DefineDCOpcodes() {
  int group;
  int column;
  int offset;
  static const uint8_t kRow[] = {
    0xC0, 0xC0, /* column groups: 0 to 7, 8 to F */
    0xE0, 0xE0,
    0xF0, 0xF0,
  };
  static const InstMnemonic kOpcodeName[] = {
    InstFadd, /* row 0xC0, column 0 to 7 */
    InstFmul, /* row 0xC0, column 8 to F */
    InstFsubr,
    InstFsub,
    InstFdivr,
    InstFdiv
  };

  DefineOpcodePrefix(PrefixDC);
  offset = 0;
  for (group = 0; group < 6; ++group) {
    for (column = 0; column < 8; column++) {
      DefineOpcode(kRow[group] + column + offset,
                   NACLi_X87,
                   InstFlag(OpcodePlusR),
                   kOpcodeName[group]);
      DefineOperand(OpcodeBaseMinus0 + column, OpFlag(OperandExtendsOpcode));
      DefineOperand(St_Operand, OpFlag(OpUse) | OpFlag(OpSet));
      DefineOperand(RegST0, OpFlag(OpUse));
    }
    if (offset)
      offset = 0;
    else
      offset = 8;
  }
}
