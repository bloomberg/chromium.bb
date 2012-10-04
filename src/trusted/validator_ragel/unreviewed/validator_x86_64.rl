/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/trusted/validator_ragel/unreviewed/validator_internal.h"

%%{
  machine x86_64_validator;
  alphtype unsigned char;
  variable p current_position;
  variable pe end_of_bundle;
  variable eof end_of_bundle;
  variable cs current_state;

  include byte_machine "byte_machines.rl";

  include prefix_actions
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include prefixes_parsing
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include rex_actions
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include rex_parsing
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include vex_actions_amd64
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include vex_parsing_amd64
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include displacement_fields_actions
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include displacement_fields_parsing
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include modrm_actions_amd64
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include modrm_parsing_amd64
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include operand_actions_amd64
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include immediate_fields_actions
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include immediate_fields_parsing_amd64
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  action rel8_operand {
    rel8_operand(current_position + 1, data, jump_dests, size,
                 &instruction_info_collected);
  }
  action rel16_operand {
    #error rel16_operand should never be used in nacl
  }
  action rel32_operand {
    rel32_operand(current_position + 1, data, jump_dests, size,
                  &instruction_info_collected);
  }
  include relative_fields_parsing
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include cpuid_actions
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";

  action check_access {
    check_access(instruction_start - data, base, index, restricted_register,
                 valid_targets, &instruction_info_collected);
  }

  action last_byte_is_not_immediate {
    instruction_info_collected |= LAST_BYTE_IS_NOT_IMMEDIATE;
  }

  action modifiable_instruction {
    instruction_info_collected |= MODIFIABLE_INSTRUCTION;
  }

  action process_0_operands {
    process_0_operands(&restricted_register, &instruction_info_collected);
  }
  action process_1_operand {
    process_1_operand(&restricted_register, &instruction_info_collected,
                      rex_prefix, operand_states);
  }
  action process_1_operand_zero_extends {
    process_1_operand_zero_extends(&restricted_register,
                                   &instruction_info_collected, rex_prefix,
                                   operand_states);
  }
  action process_2_operands {
    process_2_operands(&restricted_register, &instruction_info_collected,
                       rex_prefix, operand_states);
  }
  action process_2_operands_zero_extends {
    process_2_operands_zero_extends(&restricted_register,
                                    &instruction_info_collected, rex_prefix,
                                    operand_states);
  }

  include decode_x86_64 "validator_x86_64_instruction.rl";

  data16condrep = (data16 | condrep data16 | data16 condrep);
  data16rep = (data16 | rep data16 | data16 rep);

  # Special %rbp modifications without required sandboxing
  rbp_modifications =
    (b_0100_10x0 0x89 0xe5)                        | # mov %rsp,%rbp
    (b_0100_10x0 0x8b 0xec)                        # | mov %rsp,%rbp
    #(b_0100_1xx0 0x81 0xe5 any{3} (0x80 .. 0xff)) | # and $XXX,%rbp
    #(b_0100_1xx0 0x83 0xe5 (0x80 .. 0xff))          # and $XXX,%rbp
    @process_0_operands;

  # Special instructions used for %rbp sandboxing
  rbp_sandboxing =
    (b_0100_11x0 0x01 0xfd            | # add %r15,%rbp
     b_0100_10x1 0x03 0xef            | # add %r15,%rbp
     0x49 0x8d 0x2c 0x2f              | # lea (%r15,%rbp,1),%rbp
     0x4a 0x8d 0x6c 0x3d 0x00)          # lea 0x0(%rbp,%r15,1),%rbp
    @{ if (restricted_register == REG_RBP)
         instruction_info_collected |= RESTRICTED_REGISTER_USED;
       else
         instruction_info_collected |= UNRESTRICTED_RBP_PROCESSED;
       restricted_register = NO_REG;
       BitmapClearBit(valid_targets, (instruction_start - data));
    };

  # Special %rbp modifications without required sandboxing
  rsp_modifications =
    (b_0100_10x0 0x89 0xec)                        | # mov %rbp,%rsp
    (b_0100_10x0 0x8b 0xe5)                        | # mov %rbp,%rsp
    #(b_0100_1xx0 0x81 0xe4 any{3} (0x80 .. 0xff)) | # and $XXX,%rsp
    #Superfluous bits are not supported:
    # http://code.google.com/p/nativeclient/issues/detail?id=3012
    (b_0100_1000 0x83 0xe4 (0x80 .. 0xff))          # and $XXX,%rsp
    @process_0_operands;

  # Special instructions used for %rbp sandboxing
  rsp_sandboxing =
    (b_0100_11x0 0x01 0xfc            | # add %r15,%rsp
     b_0100_10x1 0x03 0xe7            | # add %r15,%rbp
     0x4a 0x8d 0x24 0x3c)               # lea (%rsp,%r15,1),%rsp
    @{ if (restricted_register == REG_RSP)
         instruction_info_collected |= RESTRICTED_REGISTER_USED;
       else
         instruction_info_collected |= UNRESTRICTED_RSP_PROCESSED;
       restricted_register = NO_REG;
       BitmapClearBit(valid_targets, (instruction_start - data));
    };

  # naclcall or nacljmp.  Note: first "and $~0x1f, %eXX" is a normal instruction
  # and as such will detect case where %rbp/%rsp is illegally modified.
  naclcall_or_nacljmp =
     # and $~0x1f, %eax/%ecx/%edx/%ebx/%esp/%ebp/%esi/%edi
     (0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7) 0xe0
     # add %r15,%rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      b_0100_11x0 0x01 (0xf8|0xf9|0xfa|0xfb|0xfc|0xfd|0xfe|0xff)
     # callq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     ((REX_WRX? 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6|0xd7)) |
     # jmpq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      (REX_WRX? 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7))))
     @{
       instruction_start -= 6;
       if (RMFromModRM(instruction_start[1]) !=
                                            RMFromModRM(instruction_start[5]) ||
           RMFromModRM(instruction_start[1]) != RMFromModRM(*current_position))
         instruction_info_collected |= UNRECOGNIZED_INSTRUCTION;
       BitmapClearBit(valid_targets, (instruction_start - data) + 3);
       BitmapClearBit(valid_targets, (instruction_start - data) + 6);
       restricted_register = NO_REG;
     } |

     # and $~0x1f, %eax/%ecx/%edx/%ebx/%esp/%ebp/%esi/%edi
     (0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7) 0xe0
     # add %r15,%rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      b_0100_10x1 0x03 (0xc7|0xcf|0xd7|0xdf|0xe7|0xef|0xf7|0xff)
     # callq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     ((REX_WRX? 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6|0xd7)) |
     # jmpq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      (REX_WRX? 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7))))
     @{
       instruction_start -= 6;
       if (RMFromModRM(instruction_start[1]) !=
                                           RegFromModRM(instruction_start[5]) ||
           RMFromModRM(instruction_start[1]) != RMFromModRM(*current_position))
         instruction_info_collected |= UNRECOGNIZED_INSTRUCTION;
       BitmapClearBit(valid_targets, (instruction_start - data) + 3);
       BitmapClearBit(valid_targets, (instruction_start - data) + 6);
       restricted_register = NO_REG;
     } |

     # rex.R?X? and $~0x1f, %eax/%ecx/%edx/%ebx/%esp/%ebp/%esi/%edi
    ((REX_RX 0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7) 0xe0
     # add %r15,%rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      b_0100_11x0 0x01 (0xf8|0xf9|0xfa|0xfb|0xfc|0xfd|0xfe|0xff)
     # callq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      ((REX_WRX? 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6|0xd7)) |
     # jmpq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      (REX_WRX? 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7)))) |

     # and $~0x1f, %r8d/%r9d/%r10d/%r11d/%r12d/%r13d/%r14d
     (b_0100_0xx1 0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6) 0xe0
     # add %r15, %r8d/%r9d/%r10d/%r11d/%r12d/%r13d/%r14d
      b_0100_11x1 0x01 (0xf8|0xf9|0xfa|0xfb|0xfc|0xfd|0xfe)
     # callq %r8/%r9/%r10/%r11/%r12/%r13/%r14
     ((b_0100_xxx1 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6)) |
     # jmpq %r8/%r9/%r10/%r11/%r12/%r13/%r14
      (b_0100_xxx1 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6)))))
     @{
       instruction_start -= 7;
       if (RMFromModRM(instruction_start[2]) !=
                                            RMFromModRM(instruction_start[6]) ||
           RMFromModRM(instruction_start[2]) != RMFromModRM(*current_position))
         instruction_info_collected |= UNRECOGNIZED_INSTRUCTION;
       BitmapClearBit(valid_targets, (instruction_start - data) + 4);
       BitmapClearBit(valid_targets, (instruction_start - data) + 7);
       restricted_register = NO_REG;
     } |

     # rex.R?X? and $~0x1f, %eax/%ecx/%edx/%ebx/%esp/%ebp/%esi/%edi
    ((REX_RX 0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7) 0xe0
     # add %r15,%rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      b_0100_10x1 0x03 (0xc7|0xcf|0xd7|0xdf|0xe7|0xef|0xf7|0xff)
     # callq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      ((REX_WRX? 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6|0xd7)) |
     # jmpq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      (REX_WRX? 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7)))) |

     # and $~0x1f, %r8d/%r9d/%r10d/%r11d/%r12d/%r13d/%r14d
     (b_0100_0xx1 0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6) 0xe0
     # add %r15, %r8d/%r9d/%r10d/%r11d/%r12d/%r13d/%r14d
      b_0100_11x1 0x03 (0xc7|0xcf|0xd7|0xdf|0xe7|0xef|0xf7)
     # callq %r8/%r9/%r10/%r11/%r12/%r13/%r14
     ((b_0100_xxx1 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6)) |
     # jmpq %r8/%r9/%r10/%r11/%r12/%r13/%r14
      (b_0100_xxx1 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6)))))
     @{
       instruction_start -= 7;
       if (RMFromModRM(instruction_start[2]) !=
                                           RegFromModRM(instruction_start[6]) ||
           RMFromModRM(instruction_start[2]) != RMFromModRM(*current_position))
         instruction_info_collected |= UNRECOGNIZED_INSTRUCTION;
       BitmapClearBit(valid_targets, (instruction_start - data) + 4);
       BitmapClearBit(valid_targets, (instruction_start - data) + 7);
       restricted_register = NO_REG;
     };

  # EMMS/SSE2/AVX instructions which have implicit %ds:(%rsi) operand
  # maskmovq %mmX,%mmY
  maskmovq =
      REX_WRXB? (0x0f 0xf7)
      @CPUFeature_EMMX modrm_registers;
  # maskmovdqu %xmmX, %xmmY
  maskmovdqu =
      0x66 REX_WRXB? (0x0f 0xf7) @not_data16_prefix
      @CPUFeature_SSE2 modrm_registers;
  # vmaskmovdqu %xmmX, %xmmY
  vmaskmovdqu =
      ((0xc4 (VEX_RB & VEX_map00001) 0x79 @vex_prefix3) |
      (0xc5 (0x79 | 0xf9) @vex_prefix_short)) 0xf7
      @CPUFeature_AVX modrm_registers;
  mmx_sse_rdi_instruction = maskmovq | maskmovdqu | vmaskmovdqu;

  # String instructions which use only %ds:(%rsi)
  string_instruction_rsi_no_rdi =
    (rep? 0xac                 | # lods   %ds:(%rsi),%al
     data16rep 0xad            | # lods   %ds:(%rsi),%ax
     rep? REXW_NONE? 0xad)     ; # lods   %ds:(%rsi),%eax/%rax

  # String instructions which use only %ds:(%rdi)
  string_instruction_rdi_no_rsi =
    condrep? 0xae             | # scas   %es:(%rdi),%al
    data16condrep 0xaf        | # scas   %es:(%rdi),%ax
    condrep? REXW_NONE? 0xaf  | # scas   %es:(%rdi),%eax/%rax

    rep? 0xaa                 | # stos   %al,%es:(%rdi)
    data16rep 0xab            | # stos   %ax,%es:(%rdi)
    rep? REXW_NONE? 0xab      ; # stos   %eax/%rax,%es:(%rdi)

  # String instructions which use both %ds:(%rsi) and %ds:(%rdi)
  string_instruction_rsi_rdi =
    condrep? 0xa6            | # cmpsb    %es:(%rdi),%ds:(%rsi)
    data16condrep 0xa7       | # cmpsw    %es:(%rdi),%ds:(%rsi)
    condrep? REXW_NONE? 0xa7 | # cmps[lq] %es:(%rdi),%ds:(%rsi)

    rep? 0xa4                | # movsb    %es:(%rdi),%ds:(%rsi)
    data16rep 0xa5           | # movsw    %es:(%rdi),%ds:(%rsi)
    rep? REXW_NONE? 0xa5     ; # movs[lq] %es:(%rdi),%ds:(%rsi)

  sandbox_instruction_rsi_no_rdi =
    (0x89 | 0x8b) 0xf6       . # mov %esi,%esi
    0x49 0x8d 0x34 0x37      . # lea (%r15,%rsi,1),%rsi
    string_instruction_rsi_no_rdi
    @{
       instruction_start -= 6;
       BitmapClearBit(valid_targets, (instruction_start - data) + 2);
       BitmapClearBit(valid_targets, (instruction_start - data) + 6);
       restricted_register = NO_REG;
    } |

    REX_X (0x89 | 0x8b) 0xf6 . # mov %esi,%esi
    0x49 0x8d 0x34 0x37      . # lea (%r15,%rsi,1),%rsi
    string_instruction_rsi_no_rdi
    @{
       instruction_start -= 7;
       BitmapClearBit(valid_targets, (instruction_start - data) + 3);
       BitmapClearBit(valid_targets, (instruction_start - data) + 7);
       restricted_register = NO_REG;
    };

  sandbox_instruction_rdi_no_rsi =
    (0x89 | 0x8b) 0xff       . # mov %edi,%edi
    0x49 0x8d 0x3c 0x3f      . # lea (%r15,%rdi,1),%rdi
    (string_instruction_rdi_no_rsi | mmx_sse_rdi_instruction)
    @{
       instruction_start -= 6;
       BitmapClearBit(valid_targets, (instruction_start - data) + 2);
       BitmapClearBit(valid_targets, (instruction_start - data) + 6);
       restricted_register = NO_REG;
    } |

    REX_X (0x89 | 0x8b) 0xff . # mov %edi,%edi
    0x49 0x8d 0x3c 0x3f      . # lea (%r15,%rdi,1),%rdi
    (string_instruction_rdi_no_rsi | mmx_sse_rdi_instruction)
    @{
       instruction_start -= 7;
       BitmapClearBit(valid_targets, (instruction_start - data) + 3);
       BitmapClearBit(valid_targets, (instruction_start - data) + 7);
       restricted_register = NO_REG;
    };


  # String instructions which use both %ds:(%rsi) and %ds:(%rdi)
  sandbox_instruction_rsi_rdi =
    (0x89 | 0x8b) 0xf6       . # mov %esi,%esi
    0x49 0x8d 0x34 0x37      . # lea (%r15,%rsi,1),%rsi
    (0x89 | 0x8b) 0xff       . # mov %edi,%edi
    0x49 0x8d 0x3c 0x3f      . # lea (%r15,%rdi,1),%rdi
    string_instruction_rsi_rdi
    @{
       instruction_start -= 12;
       BitmapClearBit(valid_targets, (instruction_start - data) + 2);
       BitmapClearBit(valid_targets, (instruction_start - data) + 6);
       BitmapClearBit(valid_targets, (instruction_start - data) + 8);
       BitmapClearBit(valid_targets, (instruction_start - data) + 12);
       restricted_register = NO_REG;
    } |

    (0x89 | 0x8b) 0xf6       . # mov %esi,%esi
    0x49 0x8d 0x34 0x37      . # lea (%r15,%rsi,1),%rsi
    REX_X (0x89 | 0x8b) 0xff . # mov %edi,%edi
    0x49 0x8d 0x3c 0x3f      . # lea (%r15,%rdi,1),%rdi
    string_instruction_rsi_rdi
    @{
       instruction_start -= 13;
       BitmapClearBit(valid_targets, (instruction_start - data) + 2);
       BitmapClearBit(valid_targets, (instruction_start - data) + 6);
       BitmapClearBit(valid_targets, (instruction_start - data) + 9);
       BitmapClearBit(valid_targets, (instruction_start - data) + 13);
       restricted_register = NO_REG;
    } |

    REX_X (0x89 | 0x8b) 0xf6 . # mov %esi,%esi
    0x49 0x8d 0x34 0x37      . # lea (%r15,%rsi,1),%rsi
    (0x89 | 0x8b) 0xff       . # mov %edi,%edi
    0x49 0x8d 0x3c 0x3f      . # lea (%r15,%rdi,1),%rdi
    string_instruction_rsi_rdi
    @{
       instruction_start -= 13;
       BitmapClearBit(valid_targets, (instruction_start - data) + 3);
       BitmapClearBit(valid_targets, (instruction_start - data) + 7);
       BitmapClearBit(valid_targets, (instruction_start - data) + 9);
       BitmapClearBit(valid_targets, (instruction_start - data) + 13);
       restricted_register = NO_REG;
    } |

    REX_X (0x89 | 0x8b) 0xf6 . # mov %esi,%esi
    0x49 0x8d 0x34 0x37      . # lea (%r15,%rsi,1),%rsi
    REX_X (0x89 | 0x8b) 0xff . # mov %edi,%edi
    0x49 0x8d 0x3c 0x3f      . # lea (%r15,%rdi,1),%rdi
    string_instruction_rsi_rdi
    @{
       instruction_start -= 14;
       BitmapClearBit(valid_targets, (instruction_start - data) + 3);
       BitmapClearBit(valid_targets, (instruction_start - data) + 7);
       BitmapClearBit(valid_targets, (instruction_start - data) + 10);
       BitmapClearBit(valid_targets, (instruction_start - data) + 14);
       restricted_register = NO_REG;
    };

  special_instruction =
    (rbp_modifications |
     rsp_modifications |
     rbp_sandboxing |
     rsp_sandboxing |
     naclcall_or_nacljmp |
     sandbox_instruction_rsi_no_rdi |
     sandbox_instruction_rdi_no_rsi |
     sandbox_instruction_rsi_rdi)
    @{
       instruction_info_collected |= SPECIAL_INSTRUCTION;
    };

  # Remove special instructions which are only allowed in special cases.
  normal_instruction = one_instruction - special_instruction;

  # Check if call is properly aligned
  call_alignment =
    ((normal_instruction &
      # Direct call
      ((data16 REX_RXB? 0xe8 rel16) |
       (REX_WRXB? 0xe8 rel32) |
       (data16 REXW_RXB 0xe8 rel32))) |
     (special_instruction &
      # Indirect call
      (any* data16? REX_WRXB? 0xff ((opcode_2 | opcode_3) any* &
                                    (modrm_memory | modrm_registers)))))
    @{
      if (((current_position - data) & kBundleMask) != kBundleMask)
        instruction_info_collected |= BAD_CALL_ALIGNMENT;
    };


  main := ((call_alignment | normal_instruction | special_instruction)
     >{
       BitmapSetBit(valid_targets, current_position - data);
     }
     @{
       if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
           (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
         result &= user_callback(
             instruction_start, current_position,
             instruction_info_collected |
             ((restricted_register << RESTRICTED_REGISTER_SHIFT) &
              RESTRICTED_REGISTER_MASK), callback_data);
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       instruction_info_collected = 0;
       SET_REX_PREFIX(FALSE);
       SET_VEX_PREFIX2(0xe0);
       SET_VEX_PREFIX3(0x00);
       operand_states = 0;
     })*
    $err{
        result &= user_callback(instruction_start, current_position,
                                UNRECOGNIZED_INSTRUCTION, callback_data);
        continue;
    };

}%%

%% write data;

Bool ValidateChunkAMD64(const uint8_t *data, size_t size,
                        enum validation_options options,
                        const NaClCPUFeaturesX86 *cpu_features,
                        validation_callback_func user_callback,
                        void *callback_data) {
  bitmap_word valid_targets_small;
  bitmap_word jump_dests_small;
  bitmap_word *valid_targets;
  bitmap_word *jump_dests;
  const uint8_t *current_position;
  const uint8_t *end_of_bundle;
  int result = TRUE;

  CHECK(sizeof valid_targets_small == sizeof jump_dests_small);
  CHECK(size % kBundleSize == 0);

  /* For a very small sequence (one bundle) malloc is too expensive.  */
  if (size <= sizeof valid_targets_small) {
    valid_targets_small = 0;
    valid_targets = &valid_targets_small;
    jump_dests_small = 0;
    jump_dests = &jump_dests_small;
  } else {
    valid_targets = BitmapAllocate(size);
    jump_dests = BitmapAllocate(size);
    if (!valid_targets || !jump_dests) {
      free(jump_dests);
      free(valid_targets);
      errno = ENOMEM;
      return FALSE;
    }
  }

  if (options & PROCESS_CHUNK_AS_A_CONTIGUOUS_STREAM)
    end_of_bundle = data + size;
  else
    end_of_bundle = data + kBundleSize;

  for (current_position = data;
       current_position < data + size;
       current_position = end_of_bundle,
       end_of_bundle = current_position + kBundleSize) {
    /* Start of the instruction being processed.  */
    const uint8_t *instruction_start = current_position;
    int current_state;
    uint32_t instruction_info_collected = 0;
    /* Keeps one byte of information per operand in the current instruction:
     *  2 bits for register kinds,
     *  5 bits for register numbers (16 regs plus RIZ). */
    uint32_t operand_states = 0;
    enum register_name base = NO_REG;
    enum register_name index = NO_REG;
    enum register_name restricted_register = NO_REG;
    uint8_t rex_prefix = FALSE;
    uint8_t vex_prefix2 = 0xe0;
    uint8_t vex_prefix3 = 0x00;

    %% write init;
    %% write exec;

    if (restricted_register == REG_RBP)
      result &= user_callback(end_of_bundle, end_of_bundle,
                              RESTRICTED_RBP_UNPROCESSED |
                              ((REG_RBP << RESTRICTED_REGISTER_SHIFT) &
                               RESTRICTED_REGISTER_MASK), callback_data);
    else if (restricted_register == REG_RSP)
      result &= user_callback(end_of_bundle, end_of_bundle,
                              RESTRICTED_RSP_UNPROCESSED |
                              ((REG_RSP << RESTRICTED_REGISTER_SHIFT) &
                               RESTRICTED_REGISTER_MASK), callback_data);
  }

  result &= ProcessInvalidJumpTargets(data, size, valid_targets, jump_dests,
                                      user_callback, callback_data);

  /* We only use malloc for a large code sequences  */
  if (size > sizeof valid_targets_small) {
    free(jump_dests);
    free(valid_targets);
  }
  if (!result) errno = EINVAL;
  return result;
}
