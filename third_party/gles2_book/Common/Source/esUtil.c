//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

// ESUtil.c
//
//    A utility library for OpenGL ES.  This library provides a
//    basic common framework for the example applications in the
//    OpenGL ES 2.0 Programming Guide.
//

///
//  Includes
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <GLES2/gl2.h>

#include "esUtil.h"
#include "esUtil_win.h"

///
//  esInitContext()
//
//      Initialize ES utility context.  This must be called before calling any other
//      functions.
//
void esInitContext ( ESContext *esContext )
{
   if ( esContext != NULL )
   {
      memset( esContext, 0, sizeof( ESContext) );
   }
}

///
// esLogMessage()
//
//    Log an error message to the debug output for the platform
//
void esLogMessage ( const char *formatStr, ... )
{
    va_list params;
    char buf[BUFSIZ];

    va_start ( params, formatStr );
    vsprintf_s ( buf, sizeof(buf),  formatStr, params );
    
    printf ( "%s", buf );
    
    va_end ( params );
}

///
// esLoadTGA()
//
//    Loads a 24-bit TGA image from a file
//
char* esLoadTGA ( char *fileName, int *width, int *height )
{
   char *buffer;

   if ( WinTGALoad ( fileName, &buffer, width, height ) )
   {
      return buffer;
   }

   return NULL;
}
