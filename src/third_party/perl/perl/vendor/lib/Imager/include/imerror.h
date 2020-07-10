#ifndef IMAGER_IMERROR_H
#define IMAGER_IMERROR_H

/* error handling 
   see error.c for documentation
   the error information is currently global
*/
typedef void (*i_error_cb)(int code, char const *msg);
typedef void (*i_failed_cb)(i_errmsg *msgs);
extern i_error_cb i_set_error_cb(i_error_cb);
extern i_failed_cb i_set_failed_cb(i_failed_cb);
extern void i_set_argv0(char const *);
extern int i_set_errors_fatal(int new_fatal);
extern i_errmsg *im_errors(pIMCTX);

extern void im_push_error(pIMCTX, int code, char const *msg);
#ifndef IMAGER_NO_CONTEXT
extern void i_push_errorf(int code, char const *fmt, ...) I_FORMAT_ATTR(2, 3);
#endif
extern void im_push_errorf(pIMCTX, int code, char const *fmt, ...) I_FORMAT_ATTR(3, 4);
extern void im_push_errorvf(im_context_t ctx, int code, char const *fmt, va_list);
extern void im_clear_error(pIMCTX);
extern int i_failed(int code, char const *msg);

#endif
