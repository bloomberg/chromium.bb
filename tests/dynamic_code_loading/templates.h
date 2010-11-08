/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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

/*
 * The end of the text segment, the dynamic code resion starts at the
 * next page.
 */
extern char etext[];
