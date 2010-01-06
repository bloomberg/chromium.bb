//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

//
// Hello_Triangle.h
//
//    This is a simple example that draws a single triangle with
//    a minimal vertex/fragment shader.  The purpose of this 
//    example is to demonstrate the basic concepts of 
//    OpenGL ES 2.0 rendering.

#ifndef HELLO_TRIANGLE_H
#define HELLO_TRIANGLE_H

#include "esUtil.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct
{
   // Handle to a program object
   GLuint programObject;
   // Handle to vbo object
   GLuint vbo;

} HTUserData;

extern int htInit ( ESContext *esContext );

extern void htDraw ( ESContext *esContext );

extern void htShutDown ( ESContext *esContext );

#ifdef __cplusplus
}
#endif  // __cplusplus
#endif  // HELLO_TRIANGLE_H
