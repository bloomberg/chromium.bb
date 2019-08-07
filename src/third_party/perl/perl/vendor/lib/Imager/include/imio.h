#ifndef IMAGER_IMIO_H_
#define IMAGER_IMIO_H_
#include <stdio.h>
#include <sys/stat.h>

#include "imconfig.h"
#include "log.h"

typedef struct i_mempool {
  void **p;
  unsigned int alloc;
  unsigned int used;
} i_mempool;

void  i_mempool_init(i_mempool *mp);
void  i_mempool_extend(i_mempool *mp);
void *i_mempool_alloc(i_mempool *mp, size_t size);
void  i_mempool_destroy(i_mempool *mp);

#ifdef _MSC_VER
#undef min
#undef max
#endif

extern unsigned long i_utf8_advance(char const **p, size_t *len);

#endif
