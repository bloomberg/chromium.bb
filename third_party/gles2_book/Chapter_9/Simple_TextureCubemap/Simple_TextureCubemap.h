//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

#ifndef SIMPLE_TEXTURE_CUBEMAP_H
#define SIMPLE_TEXTURE_CUBEMAP_H

#include "esUtil.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct
{
   // Handle to a program object
   GLuint programObject;

   // Attribute locations
   GLint  positionLoc;
   GLint  normalLoc;

   // Sampler location
   GLint samplerLoc;

   // Texture handle
   GLuint textureId;

   // Vertex data
   int    numIndices;
   GLuint vboIds[3];
} STCUserData;

extern int stcInit ( ESContext *esContext );

extern void stcDraw ( ESContext *esContext );

extern void stcShutDown ( ESContext *esContext );

#ifdef __cplusplus
}
#endif  // __cplusplus
#endif  // SIMPLE_TEXTURE_CUBEMAP_H
