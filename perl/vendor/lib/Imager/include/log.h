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

int i_init_log( const char *name, int onoff );
void i_fatal ( int exitcode,const char *fmt, ... );
void i_lhead ( const char *file, int line  );
void i_loog(int level,const char *msg, ... ) I_FORMAT_ATTR(2,3);

/*
=item mm_log((level, format, ...))
=category Logging

This is the main entry point to logging. Note that the extra set of
parentheses are required due to limitations in C89 macros.

This will format a string with the current file and line number to the
log file if logging is enabled.

=cut
*/

#ifdef IMAGER_LOG
#define mm_log(x) { i_lhead(__FILE__,__LINE__); i_loog x; } 
#else
#define mm_log(x)
#endif


#endif /* _LOG_H_ */
