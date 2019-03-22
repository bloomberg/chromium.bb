#ifndef _REGMACH_H_
#define _REGMACH_H_

#include <stdio.h>
#include <math.h>
#include "imager.h"

enum rm_byte_codes {
  rbc_add, /* ra + rb -> r*/
  rbc_subtract, /* ra - rb -> r */
  rbc_mult, /* ra * rb -> r */
  rbc_div, /* ra / rb -> r */
  rbc_mod, /* ra % rb -> r */
  rbc_pow, /* ra ** rb -> r */
  rbc_uminus, /* -ra -> r */
  rbc_multp, /* pa ** rb -> p */
  rbc_addp, /* pa + pb -> p */
  rbc_subtractp, /* pa - pb -> p */
  /* rbcParm, we just preload a register */
  rbc_sin, /* sin(ra) -> r */
  rbc_cos, /* cos(ra) -> r */
  rbc_atan2, /* atan2(ra,rb) -> r */
  rbc_sqrt, /* sqrt(ra) -> r */
  rbc_distance, /* distance(rx, ry, rx, ry) -> r */
  /* getp? codes must be in order */
  rbc_getp1, /* getp1(ra, rb) -> p */
  rbc_getp2, /* getp2(ra, rb) -> p */
  rbc_getp3, /* getp3(ra, rb) -> p */
  rbc_value, /* value(pa) -> r */
  rbc_hue, /* hue(pa) -> r */
  rbc_sat, /* sat(pa) -> r */
  rbc_hsv, /* hsv(rh, rs, rv) -> p */
  rbc_red, /* red(pa) -> r */
  rbc_green, /* green(pa) -> r */
  rbc_blue, /* blue(pa) -> r */
  rbc_rgb, /* rgb(rr, rg, rb) -> p */
  rbc_int, /* int(ra) -> r */
  rbc_if, /* if(rc, rt, rf) -> r */
  rbc_ifp, /* if(rc, pt, pf) -> p */
  rbc_le, /* ra <= rb -> r */
  rbc_lt, /* ra < rb -> r */
  rbc_ge, /* ra >= rb -> r */
  rbc_gt, /* ra > rb -> r */
  rbc_eq, /* ra == rb -> r -- does approx equal */
  rbc_ne, /* ra != rb -> r -- does approx equal */
  rbc_and, /* ra && rb -> r */
  rbc_or, /* ra || rb -> r */
  rbc_not, /* !ra -> r */
  rbc_abs, /* abs(ra) -> r */
  rbc_ret, /* returns pa */
  rbc_jump, /* jump to ja */
  rbc_jumpz, /* jump if ra == 0 to jb */
  rbc_jumpnz, /* jump if ra != 0 to jb */
  rbc_set, /* ra -> r */
  rbc_setp, /* pa -> p*/
  rbc_print, /* print(ra) -> r -- prints, leaves on stack */
  rbc_rgba, /* rgba(ra, rb, rc, rd) -> p */
  rbc_hsva, /* hsva(ra, rb, rc, rd) -> p */
  rbc_alpha, /* alpha(pa) -> r */
  rbc_log, /* log(ra) -> r */
  rbc_exp, /* exp(ra) -> r */
  rbc_det, /* det(ra, rb, rc, rd) -> r */
  rbc_op_count
};

/* rm_word was originally char, but even for some simpler expressions
   I was getting close to running out of register numbers.
   It should also simplify structure alignment issues. (I hope.)
*/
typedef int rm_word;
#define RM_WORD_PACK "i"

struct rm_op {
  rm_word code; /* op code */
  rm_word ra; /* first operand */
  rm_word rb; /* possible second operand */
  rm_word rc; /* possible third operand */
  rm_word rd; /* possible fourth operand */
  rm_word rout; /* output register */
};

i_color i_rm_run(struct rm_op codes[], size_t code_count, 
		 double n_regs[], size_t n_regs_count,
		 i_color c_regs[], size_t c_regs_count,
		 i_img *images[], size_t image_count);

/* op_run(fx, sizeof(fx), parms, 2)) */

#endif /* _REGMACH_H_ */
