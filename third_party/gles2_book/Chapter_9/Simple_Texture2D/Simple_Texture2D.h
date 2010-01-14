//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

#ifndef SIMPLE_TEXTURE_2D_H
#define SIMPLE_TEXTURE_2D_H

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
   GLint  texCoordLoc;

   // Sampler location
   GLint samplerLoc;

   // Texture handle
   GLuint textureId;

   // Vertex buffer object handles
   GLuint vboIds[2];

} STUserData;

extern int stInit ( ESContext *esContext );

extern void stDraw ( ESContext *esContext );

extern void stShutDown ( ESContext *esContext );

#ifdef __cplusplus
}
#endif  // __cplusplus
#endif  // SIMPLE_TEXTURE_2D_H
