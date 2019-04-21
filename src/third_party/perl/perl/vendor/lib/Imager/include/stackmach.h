#ifndef _STACKMACH_H_
#define _STACKMACH_H_

#include <stdio.h>
#include <math.h>

enum ByteCodes {
  bcAdd,
  bcSubtract,
  bcMult,
  bcDiv,
  bcParm,
  bcSin,
  bcCos
};

double i_op_run(int codes[], size_t code_size, double parms[], size_t parm_size);

/* op_run(fx, sizeof(fx), parms, 2)) */





#endif /* _STACKMACH_H_ */
