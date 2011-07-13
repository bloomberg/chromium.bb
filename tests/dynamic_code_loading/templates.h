/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


extern char template_func;
extern char template_func_end;
extern char template_func_replacement;
extern char template_func_replacement_end;
extern char template_func_nonreplacement;
extern char template_func_nonreplacement_end;
extern char hlts;
extern char hlts_end;
extern char invalid_code;
extern char invalid_code_end;
extern char branch_forwards;
extern char branch_forwards_end;
extern char branch_backwards;
extern char branch_backwards_end;
extern char template_func_misaligned_replacement;
extern char template_func_misaligned_replacement_end;
extern char template_func_illegal_call_target;
extern char template_func_illegal_call_target_end;
extern char template_func_illegal_register_replacement;
extern char template_func_illegal_register_replacement_end;
extern char template_func_illegal_guard_replacement;
extern char template_func_illegal_guard_replacement_end;
extern char template_func_illegal_constant_replacement;
extern char template_func_illegal_constant_replacement_end;
#if defined(__i386__) || defined(__x86_64__)
extern char template_instr;
extern char template_instr_end;
extern char template_instr_replace;
extern char template_instr_replace_end;
#endif

extern char template_func_external_jump_target;
extern char template_func_external_jump_target_end;
extern char template_func_external_jump_target_replace;
extern char template_func_external_jump_target_replace_end;
