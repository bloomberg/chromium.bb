#ifndef IMAGER_IMERROR_H
#define IMAGER_IMERROR_H

/* error handling 
   see error.c for documentation
   the error information is currently global
*/
typedef struct {
  char *msg;
  int code;
} i_errmsg;

typedef void (*i_error_cb)(int code, char const *msg);
typedef void (*i_failed_cb)(i_errmsg *msgs);
extern i_error_cb i_set_error_cb(i_error_cb);
extern i_failed_cb i_set_failed_cb(i_failed_cb);
extern void i_set_argv0(char const *);
extern int i_set_errors_fatal(int new_fatal);
extern i_errmsg *i_errors(void);

extern void i_push_error(int code, char const *msg);
extern void i_push_errorf(int code, char const *fmt, ...) I_FORMAT_ATTR(2, 3);
extern void i_push_errorvf(int code, char const *fmt, va_list);
extern void i_clear_error(void);
extern int i_failed(int code, char const *msg);

#endif
