#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "imdatatypes.h"
/* 
   input:  name of file to log too
   input:  onoff, 0 means no logging
   global: creates a global variable FILE* lg_file
*/

int im_init_log(pIMCTX, const char *name, int onoff );
#define i_init_log(name, onoff) im_init_log(aIMCTX, name, onoff)
#ifndef IMAGER_NO_CONTEXT
void i_fatal ( int exitcode,const char *fmt, ... );
#endif
void im_fatal (pIMCTX, int exitcode,const char *fmt, ... );
void im_lhead ( pIMCTX, const char *file, int line  );
void i_lhead ( const char *file, int line  );
void i_loog(int level,const char *msg, ... ) I_FORMAT_ATTR(2,3);
void im_loog(pIMCTX, int level,const char *msg, ... ) I_FORMAT_ATTR(3,4);

/*
=item im_log((aIMCTX, level, format, ...))
=category Logging

This is the main entry point to logging. Note that the extra set of
parentheses are required due to limitations in C89 macros.

This will format a string with the current file and line number to the
log file if logging is enabled.

This must be called with a context object defined by one of the
C<dIMCTX> macros in scope.

This can also be called as C<mm_log((level, format, args))> in which
case the currently active context is used and any in scope context is
ignored.

=cut
*/

#ifdef IMAGER_LOG
#ifndef IMAGER_NO_CONTEXT
#define mm_log(x) { i_lhead(__FILE__,__LINE__); i_loog x; } 
#endif
#define im_log(x) { im_lhead(aIMCTX, __FILE__,__LINE__); im_loog x; } 
#else
#define mm_log(x)
#define im_log(x)
#endif


#endif /* _LOG_H_ */
