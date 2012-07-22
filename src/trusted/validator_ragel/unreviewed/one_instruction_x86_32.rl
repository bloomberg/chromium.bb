/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

%%{
  machine one_instruction_x86_32;
  alphtype unsigned char;

  # We need to know where DISP, IMM, and REL fields can be found
  action disp8_operand_begin { }
  action disp8_operand_end { }
  action disp32_operand_begin { }
  action disp32_operand_end { }
  action disp64_operand_begin { }
  action disp64_operand_end { }

  action imm8_operand_begin { }
  action imm8_operand_end { }
  action imm16_operand_begin { }
  action imm16_operand_end { }
  action imm32_operand_begin { }
  action imm32_operand_end { }
  action imm64_operand_begin { }
  action imm64_operand_end { }

  action rel8_operand_begin { }
  action rel8_operand_end { }
  action rel16_operand_begin { }
  action rel16_operand_end { }
  action rel32_operand_begin { }
  action rel32_operand_end { }

  include decode_x86_32 "one_valid_instruction_x86_32.rl";

  main := one_instruction;

}%%
