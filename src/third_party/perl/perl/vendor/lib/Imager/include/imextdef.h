#ifndef IMAGER_IMEXTDEF_H
#define IMAGER_IMEXTDEF_H

#include "imexttypes.h"

extern im_ext_funcs imager_function_table;

#define PERL_SET_GLOBAL_CALLBACKS \
  sv_setiv(get_sv(PERL_FUNCTION_TABLE_NAME, 1), PTR2IV(&imager_function_table));

#endif
