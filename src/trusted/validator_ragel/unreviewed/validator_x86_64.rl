/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/trusted/validator_ragel/unreviewed/validator_internal.h"

#include "native_client/src/trusted/validator_ragel/gen/validator_x86_64_instruction_consts.c"

%%{
  machine x86_64_validator;
  alphtype unsigned char;
  variable p current_position;
  variable pe end_of_bundle;
  variable eof end_of_bundle;
  variable cs current_state;

  action check_access {
    check_access(instruction_start - data, base, index, restricted_register, valid_targets,
                 &instruction_info_collected);
  }

  action rel8_operand {
    instruction_info_collected |= RELATIVE_8BIT;
    rel8_operand(current_position + 1, data, jump_dests, size,
                 &instruction_info_collected);
  }
  action rel16_operand {
    #error rel16_operand should never be used in nacl
  }
  action rel32_operand {
    instruction_info_collected |= RELATIVE_32BIT;
    rel32_operand(current_position + 1, data, jump_dests, size,
                  &instruction_info_collected);
  }

  action opcode_in_imm {
    instruction_info_collected |= IMMEDIATE_IS_OPCODE;
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
    ((0x48 | 0x4a) 0x89 0xe5)               | # mov %rsp,%rbp
    ((0x48 | 0x4a) 0x8b 0xec)               # | mov %rsp,%rbp
    #(0x48 0x81 0xe5 any{3} (0x80 .. 0xff)) | # and $XXX,%rbp
    #(0x48 0x83 0xe5 (0x80 .. 0xff))          # and $XXX,%rbp
    @process_0_operands;

  # Special instructions used for %rbp sandboxing
  rbp_sandboxing =
    (0x4c 0x01 0xfd            | # add %r15,%rbp
     0x49 0x8d 0x2c 0x2f       | # lea (%r15,%rbp,1),%rbp
     0x4a 0x8d 0x6c 0x3d 0x00)   # lea 0x0(%rbp,%r15,1),%rbp
    @{ if (restricted_register != REG_RBP) {
         instruction_info_collected |= UNRESTRICTED_RBP_PROCESSED;
       }
       restricted_register = NO_REG;
       BitmapClearBit(valid_targets, (instruction_start - data));
    };

  # Special %rbp modifications without required sandboxing
  rsp_modifications =
    ((0x48 | 0x4a) 0x89 0xec)              | # mov %rbp,%rsp
    ((0x48 | 0x4a) 0x8b 0xe5)              | # mov %rbp,%rsp
    (0x48 0x81 0xe4 any{3} (0x80 .. 0xff)) | # and $XXX,%rsp
    (0x48 0x83 0xe4 (0x80 .. 0xff))          # and $XXX,%rsp
    @process_0_operands;

  # Special instructions used for %rbp sandboxing
  rsp_sandboxing =
    (0x4c 0x01 0xfc       | # add %r15,%rsp
     0x4a 0x8d 0x24 0x3c)   # lea (%rsp,%r15,1),%rsp
    @{ if (restricted_register != REG_RSP) {
         instruction_info_collected |= UNRESTRICTED_RSP_PROCESSED;
       }
       restricted_register = NO_REG;
       BitmapClearBit(valid_targets, (instruction_start - data));
    };

  # naclcall - old version (is not used by contemporary toolchain)
  # Note: first "and $~0x1f, %eXX" is normal instruction and as such will detect
  # case where %rbp/%rsp is illegally modified.
  old_naclcall_or_nacljmp =

     # and $~0x1f, %eax/%ecx/%edx/%ebx/%esp/%ebp/%esi/%edi
    ((0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7) 0xe0
     # lea (%r15,%rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi),
     #   %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     0x49 0x8d (0x04|0x0c|0x14|0x1c|0x24|0x2c|0x34|0x3c)
         (0x07|0x0f|0x17|0x1f|0x27|0x2f|0x37|0x3f)
     # callq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     ((REX_WRX? 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6|0xd7)) |
     # jmpq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      (REX_WRX? 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7)))) |

     # and $~0x1f, %eax/%ecx/%edx/%ebx/%esp/%ebp/%esi/%edi
     (0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7) 0xe0
     # lea (%r15,%rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi),
     #   %r8/%r9/%r10/%r11/%r12/%r13/%r14
     0x4d 0x8d (0x04|0x0c|0x14|0x1c|0x24|0x2c|0x34)
         (0x07|0x0f|0x17|0x1f|0x27|0x2f|0x37|0x3f)
     # callq %r8/%r9/%r10/%r11/%r12/%r13/%r14
     (((REX_WRXB - REX_WRX) 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6)) |
     # jmpq %r8/%r9/%r10/%r11/%r12/%r13/%r14
      ((REX_WRXB - REX_WRX) 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6)))))
     @{
       instruction_start -= 7;
       if (((instruction_start[1] & 0x07) !=
                                        ((instruction_start[6] & 0x38) >> 3)) ||
            (((instruction_start[5] & 0x38) >> 3) !=
                                                    (*current_position & 0x07)))
         instruction_info_collected |= UNRECOGNIZED_INSTRUCTION;
       BitmapClearBit(valid_targets, (instruction_start - data) + 3);
       BitmapClearBit(valid_targets, (instruction_start - data) + 7);
       restricted_register = NO_REG;
     } |

     # rex.R?X? and $~0x1f, %eax/%ecx/%edx/%ebx/%esp/%ebp/%esi/%edi
    ((REX_RX 0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7) 0xe0
     # lea (%r15,%rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi),
     #   %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     0x49 0x8d (0x04|0x0c|0x14|0x1c|0x24|0x2c|0x34|0x3c)
         (0x07|0x0f|0x17|0x1f|0x27|0x2f|0x37|0x3f)
     # callq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     ((REX_WRX? 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6|0xd7)) |
     # jmpq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      (REX_WRX? 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7)))) |

     # rex.R?X?B and $~0x1f, %r8d/r9d/r10d/r11d/r12d/r13d/14d
     ((REX_RXB - REX_RX) 0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6) 0xe0
     # lea (%r15,%r8/%r9/%r10/%r11/%r12/%r13/%r14),
     #   %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     0x4b 0x8d (0x04|0x0c|0x14|0x1c|0x24|0x2c|0x34|0x3c)
         (0x07|0x0f|0x17|0x1f|0x27|0x2f|0x37)
     # callq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     ((REX_WRX? 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6|0xd7)) |
     # jmpq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      (REX_WRX? 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7))))

     # and $~0x1f, %eax/%ecx/%edx/%ebx/%esp/%ebp/%esi/%edi
     (REX_RX 0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7) 0xe0
     # lea (%r15,%rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi),
     #   %r8/%r9/%r10/%r11/%r12/%r13/%r14
     0x4d 0x8d (0x04|0x0c|0x14|0x1c|0x24|0x2c|0x34)
         (0x07|0x0f|0x17|0x1f|0x27|0x2f|0x37|0x3f)
     # callq %r8/%r9/%r10/%r11/%r12/%r13/%r14
     (((REX_WRXB - REX_WRX) 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6)) |
     # jmpq %r8/%r9/%r10/%r11/%r12/%r13/%r14
      ((REX_WRXB - REX_WRX) 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6)))) |

     # rex.R?X?B and $~0x1f, %r8d/r9d/r10d/r11d/r12d/r13d/14d
     ((REX_RXB - REX_RX) 0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6) 0xe0
     # lea (%r15,%r8/%r9/%r10/%r11/%r12/%r13/%r14),
     #   %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     0x4f 0x8d (0x04|0x0c|0x14|0x1c|0x24|0x2c|0x34|0x3c)
         (0x07|0x0f|0x17|0x1f|0x27|0x2f|0x37)
     # callq %r8/%r9/%r10/%r11/%r12/%r13/%r14
     (((REX_WRXB - REX_WRX) 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6)) |
     # callq %r8/%r9/%r10/%r11/%r12/%r13/%r14
      ((REX_WRXB - REX_WRX) 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6)))))
     @{
       instruction_start -= 8;
       if (((instruction_start[2] & 0x07) !=
                                        ((instruction_start[7] & 0x38) >> 3)) ||
            (((instruction_start[6] & 0x38) >> 3) !=
                                                    (*current_position & 0x07)))
         instruction_info_collected |= UNRECOGNIZED_INSTRUCTION;
       BitmapClearBit(valid_targets, (instruction_start - data) + 4);
       BitmapClearBit(valid_targets, (instruction_start - data) + 8);
       restricted_register = NO_REG;
     };

  # naclcall or nacljmp - this the form used by contemporary toolchain
  # Note: first "and $~0x1f, %eXX" is normal instruction and as such will detect
  # case where %rbp/%rsp is illegally modified.
  naclcall_or_nacljmp =
     # and $~0x1f, %eax/%ecx/%edx/%ebx/%esp/%ebp/%esi/%edi
     (0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7) 0xe0
     # add %r15,%rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     (REXW_RX - REXW_X) 0x01 (0xf8|0xf9|0xfa|0xfb|0xfc|0xfd|0xfe|0xff)
     # callq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     ((REX_WRX? 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6|0xd7)) |
     # jmpq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      (REX_WRX? 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7))))
     @{
       instruction_start -= 6;
       if (((instruction_start[1] & 0x07) != (instruction_start[5] & 0x07)) ||
           ((instruction_start[1] & 0x07) != (*current_position & 0x07)))
         instruction_info_collected |= UNRECOGNIZED_INSTRUCTION;
       BitmapClearBit(valid_targets, (instruction_start - data) + 3);
       BitmapClearBit(valid_targets, (instruction_start - data) + 6);
       restricted_register = NO_REG;
     } |

     # rex.R?X? and $~0x1f, %eax/%ecx/%edx/%ebx/%esp/%ebp/%esi/%edi
    ((REX_RX 0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7) 0xe0
     # add %r15,%rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     (REXW_RX - REXW_X) 0x01 (0xf8|0xf9|0xfa|0xfb|0xfc|0xfd|0xfe|0xff)
     # callq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     ((REX_WRX? 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6|0xd7)) |
     # jmpq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      (REX_WRX? 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6|0xe7)))) |

     # and $~0x1f, %r8d/%r9d/%r10d/%r11d/%r12d/%r13d/%r14d
     ((REX_RXB - REX_RX) 0x83 (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6) 0xe0
     # add %r15, %r8d/%r9d/%r10d/%r11d/%r12d/%r13d/%r14d
      ((REXW_RXB - REXW_XB) - REXW_RX) 0x01 (0xf8|0xf9|0xfa|0xfb|0xfc|0xfd|0xfe)
     # callq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
     (((REX_WRXB - REX_WRX) 0xff (0xd0|0xd1|0xd2|0xd3|0xd4|0xd5|0xd6)) |
     # jmpq %rax/%rcx/%rdx/%rbx/%rsp/%rbp/%rsi/%rdi
      ((REX_WRXB - REX_WRX) 0xff (0xe0|0xe1|0xe2|0xe3|0xe4|0xe5|0xe6)))))
     @{
       instruction_start -= 7;
       if (((instruction_start[2] & 0x07) != (instruction_start[6] & 0x07)) ||
           ((instruction_start[2] & 0x07) != (*current_position & 0x07)))
         instruction_info_collected |= UNRECOGNIZED_INSTRUCTION;
       BitmapClearBit(valid_targets, (instruction_start - data) + 4);
       BitmapClearBit(valid_targets, (instruction_start - data) + 7);
       restricted_register = NO_REG;
     };

  # EMMS/SSE2/AVX instructions which have implicit %ds:(%rsi) operand
  mmx_sse_rdi_instructions =
    (REX_WRXB? (0x0f 0xf7)
      @CPUFeature_EMMX modrm_registers | # maskmovq %mmX,%mmY
     0x66 REX_WRXB? (0x0f 0xf7) @not_data16_prefix
      @CPUFeature_SSE2 modrm_registers | # maskmovdqu %xmmX, %xmmY
     ((0xc4 (VEX_RB & VEX_map00001) 0x79 @vex_prefix3) |
      (0xc5 (0x79 | 0xf9) @vex_prefix_short)) 0xf7
      @CPUFeature_AVX modrm_registers);  # vmaskmovdqu %xmmX, %xmmY

  # String instructions which use only %ds:(%rsi)
  string_instructions_rsi_no_rdi =
    (rep? 0xac                 | # lods   %ds:(%rsi),%al
     data16rep 0xad            | # lods   %ds:(%rsi),%ax
     rep? REXW_NONE? 0xad)     ; # lods   %ds:(%rsi),%eax/%rax

  # String instructions which use only %ds:(%rdi)
  string_instructions_rdi_no_rsi =
    condrep? 0xae             | # scas   %es:(%rdi),%al
    data16condrep 0xaf        | # scas   %es:(%rdi),%ax
    condrep? REXW_NONE? 0xaf  | # scas   %es:(%rdi),%eax/%rax

    rep? 0xaa                 | # stos   %al,%es:(%rdi)
    data16rep 0xab            | # stos   %ax,%es:(%rdi)
    rep? REXW_NONE? 0xab      ; # stos   %eax/%rax,%es:(%rdi)

  # String instructions which use both %ds:(%rsi) and %ds:(%rdi)
  string_instructions_rsi_rdi =
    condrep? 0xa6            | # cmpsb    %es:(%rdi),%ds:(%rsi)
    data16condrep 0xa7       | # cmpsw    %es:(%rdi),%ds:(%rsi)
    condrep? REXW_NONE? 0xa7 | # cmps[lq] %es:(%rdi),%ds:(%rsi)

    rep? 0xa4                | # movsb    %es:(%rdi),%ds:(%rsi)
    data16rep 0xa5           | # movsw    %es:(%rdi),%ds:(%rsi)
    rep? REXW_NONE? 0xa5     ; # movs[lq] %es:(%rdi),%ds:(%rsi)

  sandbox_string_instructions_rsi_no_rdi =
    (0x89 | 0x8b) 0xf6         . # mov %esi,%esi
    0x49 0x8d 0x34 0x37        . # lea (%r15,%rsi,1),%rsi
    string_instructions_rsi_no_rdi
    @{
       BitmapClearBit(valid_targets, (instruction_start - data));
       BitmapClearBit(valid_targets, (instruction_start - 4 - data));
       instruction_start -= 6;
       restricted_register = NO_REG;
    };

  sandbox_string_instructions_rdi_no_rsi =
    (0x89 | 0x8b) 0xff         . # mov %edi,%edi
    0x49 0x8d 0x3c 0x3f        . # lea (%r15,%rdi,1),%rdi
    string_instructions_rdi_no_rsi
    @{
       BitmapClearBit(valid_targets, (instruction_start - data));
       BitmapClearBit(valid_targets, (instruction_start - 4 - data));
       instruction_start -= 6;
       restricted_register = NO_REG;
    };

  # String instructions which use both %ds:(%rsi) and %ds:(%rdi)
  sandbox_string_instructions_rsi_rdi =
    (0x89 | 0x8b) 0xf6        . # mov %esi,%esi
    0x49 0x8d 0x34 0x37       . # lea (%r15,%rsi,1),%rsi
    (0x89 | 0x8b) 0xff        . # mov %edi,%edi
    0x49 0x8d 0x3c 0x3f       . # lea (%r15,%rdi,1),%rdi
    string_instructions_rsi_rdi
    @{
       BitmapClearBit(valid_targets, (instruction_start - data));
       BitmapClearBit(valid_targets, (instruction_start - 4 - data));
       BitmapClearBit(valid_targets, (instruction_start - 6 - data));
       BitmapClearBit(valid_targets, (instruction_start - 10 - data));
       instruction_start -= 12;
       restricted_register = NO_REG;
    };

  special_instruction =
    (rbp_modifications |
     rsp_modifications |
     rbp_sandboxing |
     rsp_sandboxing |
     old_naclcall_or_nacljmp |
     naclcall_or_nacljmp |
     sandbox_string_instructions_rsi_no_rdi |
     sandbox_string_instructions_rdi_no_rsi |
     sandbox_string_instructions_rsi_rdi)
    @{
       instruction_info_collected |= SPECIAL_INSTRUCTION;
    };

  # Remove special instructions which are only allowed in special cases.
  normal_instruction = one_instruction - special_instruction;

  main := ((normal_instruction | special_instruction)
     >{
       BitmapSetBit(valid_targets, current_position - data);
     }
     @{
       if (instruction_info_collected & VALIDATION_ERRORS ||
           options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION) {
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

  CHECK(size % kBundleSize == 0);

  /* For a very small sequence (one bundle) malloc is too expensive.  */
  if (size <= sizeof(bitmap_word)) {
    valid_targets_small = 0;
    valid_targets = &valid_targets_small;
    jump_dests_small = 0;
    jump_dests = &jump_dests_small;
  } else {
    valid_targets = BitmapAllocate(size);
    jump_dests = BitmapAllocate(size);
    if (!valid_targets || !jump_dests) {
      result = FALSE;
      goto error_detected;
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

error_detected:
  /* We only use malloc for a large code sequences  */
  if (size > sizeof(bitmap_word)) {
    free(jump_dests);
    free(valid_targets);
  }
  return result;
}
