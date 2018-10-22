#include "imager.h"

#ifndef IMAGER_EXT_H
#define IMAGER_EXT_H

/* structures for passing data between Imager-plugin and the Imager-module */

typedef struct {
  char *name;
  void (*iptr)(void* ptr);
  char *pcode;
} func_ptr;


typedef struct {
  int (*getstr)(void *hv_t,char* key,char **store);
  int (*getint)(void *hv_t,char *key,int *store);
  int (*getdouble)(void *hv_t,char* key,double *store);
  int (*getvoid)(void *hv_t,char* key,void **store);
  int (*getobj)(void *hv_t,char* key,char* type,void **store);
} UTIL_table_t;

#endif
