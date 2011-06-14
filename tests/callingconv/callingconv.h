/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __TESTS_CALLING_CONV_H
#define __TESTS_CALLING_CONV_H

typedef struct {
  char a;
  short b;
} tiny_t;

typedef struct {
  char a;
  char b;
  int c;
  char d;
  int e;
  long long f;
  int g;
  char h;
  int i;
  char j;
  short k;
  char l;
  char m;
} big_t;

/* Comparison functions */
#define TINY_CMP(_x, _y)   (tiny_cmp((_x), (_y)))
#define BIG_CMP(_x, _y)    (big_cmp((_x), (_y)))
int tiny_cmp(const tiny_t x, const tiny_t y);
int big_cmp(const big_t x, const big_t y);


/* Setters */
#define SET_TINY_T(obj, a, b)  \
    (set_tiny_t(&(obj), a, b))

#define SET_BIG_T(obj, a, b, c, d, e, f, g, h, i, j, k, l, m) \
    (set_big_t(&(obj), a, b, c, d, e, f, g, h, i, j, k, l, m))

void set_tiny_t(tiny_t *ptr, char a, short b);

void set_big_t(big_t *ptr, char a, char b, int c, char d, int e, long long f,
               int g, char h, int i, char j, short k, char l, char m);

/* Used by the modules to keep track of the current location */
extern int current_module;
extern int current_call;
extern int current_function;
extern int *current_index_p;
extern int assert_count;

#define SET_CURRENT_MODULE(id)   (current_module = (id))
#define SET_CURRENT_FUNCTION(id) (current_function = (id))
#define SET_CURRENT_CALL(id)     (current_call = (id))
#define SET_INDEX_VARIABLE(id)   (current_index_p = &(id))

/* Used by the modules to compare arguments to the expected value */
#define ASSERT(cond)          (assert_func((cond), #cond, __FILE__, __LINE__))

void assert_func(int condition, const char *expr, const char *file, int line);

#endif
