#ifndef IMAGER_IMEXTPL_H_
#define IMAGER_IMEXTPL_H_

#include "imextpltypes.h"
#include "immacros.h"

extern im_pl_ext_funcs *imager_perl_function_ext_table;

#define DEFINE_IMAGER_PERL_CALLBACKS im_pl_ext_funcs *imager_perl_function_ext_table

#ifndef IMAGER_MIN_PL_API_LEVEL
#define IMAGER_MIN_PL_API_LEVEL IMAGER_PL_API_LEVEL
#endif

#define PERL_INITIALIZE_IMAGER_PERL_CALLBACKS \
  do {  \
    imager_perl_function_ext_table = INT2PTR(im_pl_ext_funcs *, SvIV(get_sv(PERL_PL_FUNCTION_TABLE_NAME, 1))); \
    if (!imager_perl_function_ext_table) \
      croak("Imager Perl API function table not found!"); \
    if (imager_perl_function_ext_table->version != IMAGER_PL_API_VERSION) \
      croak("Imager Perl API version incorrect"); \
    if (imager_perl_function_ext_table->level < IMAGER_MIN_PL_API_LEVEL) \
      croak("perl API level %d below minimum of %d", imager_perl_function_ext_table->level, IMAGER_MIN_PL_API_LEVEL); \
  } while (0)

/* just for use here */
#define im_exttpl imager_perl_function_ext_table

#define ip_handle_quant_opts  (im_exttpl->f_ip_handle_quant_opts)
#define ip_cleanup_quant_opts  (im_exttpl->f_ip_cleanup_quant_opts)
#define ip_copy_colors_back (im_exttpl->f_ip_copy_colors_back)

#endif
