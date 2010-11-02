/*
 * tests whether our data type are lowered as expected (ILP32)
 * TODO: sizes of size_sb and size_ue are not quite right.
 */

#include <stdarg.h>
#include <setjmp.h>
#include <unwind.h>

int size_c = sizeof(char);
int size_i = sizeof(int);
int size_l = sizeof(long);

int size_p = sizeof(void*);

int size_f = sizeof(float);
int size_d = sizeof(double);

int size_vl = sizeof(va_list);
/* presumably a 5 word buffer */
/* c.f. http://llvm.org/docs/ExceptionHandling.html#llvm_eh_sjlj_setjmp */
int size_sb = sizeof(jmp_buf);
/* this should just be 4 words but we needed to hack it */
/* c.f. http://code.google.com/p/nativeclient/issues/detail?id=1107 */
int size_ue = sizeof(struct _Unwind_Exception);
