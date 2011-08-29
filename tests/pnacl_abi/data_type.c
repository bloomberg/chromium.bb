/*
 * tests whether our data type are lowered as expected (ILP32)
 * TODO: sizes of size_sb and size_ue are not quite right.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <unwind.h>

int size_c  = sizeof(char);
int size_s  = sizeof(short);
int size_i  = sizeof(int);
int size_l  = sizeof(long);
int size_ll = sizeof(long long);

int size_p = sizeof(void*);

int size_f  = sizeof(float);
int size_d  = sizeof(double);
int size_ld = sizeof(long double);

/* A 16 byte buffer to accomodate all three architectures */
int size_vl = sizeof(va_list);

/* A 1024 byte buffer to accomodate future architectures */
int size_jb = sizeof(jmp_buf);

#define SET_ALIGNMENT(T, name) \
  typedef struct { char dummy; T x; } AlignStruct_ ## name; \
  int align_ ## name = offsetof(AlignStruct_ ## name, x)

SET_ALIGNMENT(char, c);
SET_ALIGNMENT(short, s);
SET_ALIGNMENT(int, i);
SET_ALIGNMENT(long, l);
SET_ALIGNMENT(long long, ll);

SET_ALIGNMENT(void*, p);

SET_ALIGNMENT(float, f);
SET_ALIGNMENT(double, d);
SET_ALIGNMENT(long double, ld);

SET_ALIGNMENT(va_list, vl);

SET_ALIGNMENT(jmp_buf, jb);
