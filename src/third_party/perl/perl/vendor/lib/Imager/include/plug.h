#include "imdatatypes.h"
#include "immacros.h"

/* structures for passing data between Imager-plugin and the Imager-module */

#include "ext.h"


#define getINT(k,s) (util_table->getint(INP,k,s))
#define getDOUBLE(k,s) (util_table->getdouble(INP,k,s))
#define getVOID(k,s) (util_table->getvoid(INP,k,(void**)s))
#define getSTR(k,s) (util_table->getstr(INP,k,(char**)s))
#define getOBJ(k,t,s) (util_table->getobj(INP,k,t,(void**)s))

#define i_color_set(cl,r,g,b,a) (symbol_table->i_color_set(cl,r,g,b,a))
#define i_color_info(cl) (symbol_table->i_color_info(cl))

#define im_get_context() (symbol_table->im_get_context_f())
#define i_img_empty_ch(im,x,y,ch) ((symbol_table->i_img_empty_ch_f(im_get_context(), im,x,y,ch))
#define i_img_exorcise(im) (symbol_table->i_img_exorcise_f(im))
#define i_img_info(im,info) (symbol_table->i_img_info_f(im,info))

#define i_img_setmask(im,ch_mask) (symbol_table->i_img_setmask_f(im,ch_mask))
#define i_img_getmask(im) (symbol_table->i_img_getmask_f(im))

/*
Not needed?  The i_gpix() macro in image.h will call the right function
directly.
#define i_ppix(im,x,y,val) (symbol_table->i_ppix(im,x,y,val))
#define i_gpix(im,x,y,val) (symbol_table->i_gpix(im,x,y,val))
*/

#define i_box(im, x1, y1, x2, y2,val) (symbol_table->i_box(im, x1, y1, x2, y2,val))
#define i_draw(im, x1, y1, x2, y2,val) (symbol_table->i_draw(im, x1, y1, x2, y2,val))
#define i_arc(im, x, y, rad, d1, d2,val) (symbol_table->i_arc(im, x, y, rad, d1, d2,val))
#define i_copyto(im,src, x1, y1, x2, y2, tx, ty,trans) (symbol_table->i_copyto(im,src, x1, y1, x2, y2, tx, ty,trans))
#define i_rubthru(im,src, tx, ty) (symbol_table->i_rubthru(im,src, tx, ty))

#ifdef WIN32
extern char __declspec(dllexport) evalstr[];
extern func_ptr __declspec(dllexport) function_list[];
#endif
