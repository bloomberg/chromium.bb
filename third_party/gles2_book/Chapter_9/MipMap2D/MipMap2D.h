//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

#ifndef MIP_MAP_2D_H
#define MIP_MAP_2D_H

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

   // Offset location
   GLint offsetLoc;

   // Texture handle
   GLuint textureId;

   // Vertex buffer object handle
   GLuint vboIds[2];

} MMUserData;

extern int mmInit ( ESContext *esContext );

extern void mmDraw ( ESContext *esContext );

extern void mmShutDown ( ESContext *esContext );

#ifdef __cplusplus
}
#endif  // __cplusplus
#endif  // MIP_MAP_2D_H
