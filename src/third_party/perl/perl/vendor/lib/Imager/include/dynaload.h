#ifndef _DYNALOAD_H_
#define _DYNALOAD_H_

#include "log.h"

#include "EXTERN.h"
#include "perl.h"
#include "ppport.h"

#include "ext.h"

typedef struct DSO_handle_tag DSO_handle;

typedef struct {
  HV* hv;
  char *key;
  void *store;
} UTIL_args;

#if 0
int getobj(void *hv_t,char *key,char *type,void **store);
int getint(void *hv_t,char *key,int *store);
int getdouble(void *hv_t,char *key,double *store);
int getvoid(void *hv_t,char *key,void **store);
#endif

void *DSO_open(char* file,char** evalstring);
func_ptr *DSO_funclist(DSO_handle *handle);
int DSO_close(void *);
void DSO_call(DSO_handle *handle,int func_index,HV* hv);

#ifdef __EMX__ /* OS/2 */
# ifndef RTLD_LAZY
#  define RTLD_LAZY 0
# endif /* RTLD_LAZY */
int dlclose(minthandle_t);
#endif /* __EMX__ */

#ifdef DLSYMUN

#define I_EVALSTR "_evalstr"
#define I_SYMBOL_TABLE "_symbol_table"
#define I_UTIL_TABLE "_util_table"
#define I_FUNCTION_LIST "_function_list"
#define I_INSTALL_TABLES "_install_tables"

#else 

#define I_EVALSTR "evalstr"
#define I_SYMBOL_TABLE "symbol_table"
#define I_UTIL_TABLE "util_table"
#define I_FUNCTION_LIST "function_list"
#define I_INSTALL_TABLES "install_tables"

#endif

#endif /* _DYNALOAD_H_ */

