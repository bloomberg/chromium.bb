#ifndef IMAGER_IMEXTPLTYPES_H_
#define IMAGER_IMEXTPLTYPES_H_

#ifndef PERL_NO_GET_CONTEXT
#error Sorry, you need to build with PERL_NO_GET_CONTEXT
#endif

#define IMAGER_PL_API_VERSION 1

/* This file provides functions useful for external code in
   interfacing with perl - these functions aren't part of the core
   Imager API. */

#define IMAGER_PL_API_LEVEL 1

typedef struct {
  int version;
  int level;

  /* IMAGER_PL_API_LEVEL 1 functions */
  void (*f_ip_handle_quant_opts)(pTHX_ i_quantize *quant, HV *hv);
  void (*f_ip_cleanup_quant_opts)(pTHX_ i_quantize *quant);
  void (*f_ip_copy_colors_back)(pTHX_ HV *hv, i_quantize *quant);

  /* IMAGER_PL_API_LEVEL 2 functions will go here */
} im_pl_ext_funcs;

#define PERL_PL_FUNCTION_TABLE_NAME "Imager::__ext_pl_func_table"

#endif
