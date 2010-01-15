//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

// Simple_TextureCubemap.c
//
//    This is a simple example that draws a sphere with a cubemap image applied.
//
#include <stdlib.h>
#include "Simple_TextureCubemap.h"

///
// Create a simple cubemap with a 1x1 face with a different
// color for each face
static GLuint CreateSimpleTextureCubemap( )
{
   GLuint textureId;
   // Six 1x1 RGB faces
   GLubyte cubePixels[6][3] =
   {
      // Face 0 - Red
      255, 0, 0,
      // Face 1 - Green,
      0, 255, 0, 
      // Face 3 - Blue
      0, 0, 255,
      // Face 4 - Yellow
      255, 255, 0,
      // Face 5 - Purple
      255, 0, 255,
      // Face 6 - White
      255, 255, 255
   };
   
   // Generate a texture object
   glGenTextures ( 1, &textureId );

   // Bind the texture object
   glBindTexture ( GL_TEXTURE_CUBE_MAP, textureId );
   
   // Load the cube face - Positive X
   glTexImage2D ( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 1, 1, 0, 
                  GL_RGB, GL_UNSIGNED_BYTE, &cubePixels[0] );

   // Load the cube face - Negative X
   glTexImage2D ( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 1, 1, 0, 
                  GL_RGB, GL_UNSIGNED_BYTE, &cubePixels[1] );

   // Load the cube face - Positive Y
   glTexImage2D ( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 1, 1, 0, 
                  GL_RGB, GL_UNSIGNED_BYTE, &cubePixels[2] );

   // Load the cube face - Negative Y
   glTexImage2D ( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 1, 1, 0, 
                  GL_RGB, GL_UNSIGNED_BYTE, &cubePixels[3] );

   // Load the cube face - Positive Z
   glTexImage2D ( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 1, 1, 0, 
                  GL_RGB, GL_UNSIGNED_BYTE, &cubePixels[4] );

   // Load the cube face - Negative Z
   glTexImage2D ( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 1, 1, 0, 
                  GL_RGB, GL_UNSIGNED_BYTE, &cubePixels[5] );

   // Set the filtering mode
   glTexParameteri ( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
   glTexParameteri ( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

   return textureId;

}


///
// Initialize the shader and program object
//
int stcInit ( ESContext *esContext )
{
   STCUserData *userData = esContext->userData;
   int numSlices = 20;
   int numVertices = ( (numSlices / 2) + 1 ) * ( numSlices + 1 );
   GLfloat *vertices = NULL;
   GLfloat *normals = NULL;
   GLushort *indices = NULL;
   GLbyte vShaderStr[] =  
      "attribute vec4 a_position;   \n"
      "attribute vec3 a_normal;     \n"
      "varying vec3 v_normal;       \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = a_position; \n"
      "   v_normal = a_normal;      \n"
      "}                            \n";
   
   GLbyte fShaderStr[] =  
      "precision mediump float;                            \n"
      "varying vec3 v_normal;                              \n"
      "uniform samplerCube s_texture;                      \n"
      "void main()                                         \n"
      "{                                                   \n"
      "  gl_FragColor = textureCube( s_texture, v_normal );\n"
      "}                                                   \n";

   // Load the shaders and get a linked program object
   userData->programObject = esLoadProgram ( vShaderStr, fShaderStr );

   // Get the attribute locations
   userData->positionLoc = glGetAttribLocation ( userData->programObject, "a_position" );
   userData->normalLoc = glGetAttribLocation ( userData->programObject, "a_normal" );
   
   // Get the sampler locations
   userData->samplerLoc = glGetUniformLocation ( userData->programObject, "s_texture" );

   // Load the texture
   userData->textureId = CreateSimpleTextureCubemap ();

   // Generate the vertex data
   userData->numIndices = esGenSphere ( numSlices, 0.75f, &vertices, &normals, 
                                        NULL, &indices );
   glGenBuffers( 3, userData->vboIds );
   // Load vertex positions
   glBindBuffer ( GL_ARRAY_BUFFER, userData->vboIds[0] );
   glBufferData ( GL_ARRAY_BUFFER, 3 * numVertices * sizeof(GLfloat),
                  vertices, GL_STATIC_DRAW );
   // Load vertex normals
   glBindBuffer ( GL_ARRAY_BUFFER, userData->vboIds[1] );
   glBufferData ( GL_ARRAY_BUFFER, 3 * numVertices * sizeof(GLfloat),
                  normals, GL_STATIC_DRAW );
   // Load vertex indices
   glBindBuffer ( GL_ELEMENT_ARRAY_BUFFER, userData->vboIds[2] );
   glBufferData ( GL_ELEMENT_ARRAY_BUFFER, userData->numIndices * sizeof(GLushort),
                  indices, GL_STATIC_DRAW );
   if ( vertices != NULL ) free ( vertices );
   if ( normals != NULL ) free ( normals );
   if ( indices != NULL ) free ( indices );
   
   glClearColor ( 0.0f, 0.0f, 0.0f, 0.0f );
   return TRUE;
}

///
// Draw a triangle using the shader pair created in Init()
//
void stcDraw ( ESContext *esContext )
{
   STCUserData *userData = esContext->userData;
      
   // Set the viewport
   glViewport ( 0, 0, esContext->width, esContext->height );
   
   // Clear the color buffer
   glClear ( GL_COLOR_BUFFER_BIT );

   
   glCullFace ( GL_BACK );
   glEnable ( GL_CULL_FACE );
   
   // Use the program object
   glUseProgram ( userData->programObject );

   // Load the vertex position
   glVertexAttribPointer ( userData->positionLoc, 3, GL_FLOAT, 
                           GL_FALSE, 0, 0 );
   // Load the normal
   glVertexAttribPointer ( userData->normalLoc, 3, GL_FLOAT,
                           GL_FALSE, 0, 0 );

   glEnableVertexAttribArray ( userData->positionLoc );
   glEnableVertexAttribArray ( userData->normalLoc );

   // Bind the texture
   glActiveTexture ( GL_TEXTURE0 );
   glBindTexture ( GL_TEXTURE_CUBE_MAP, userData->textureId );

   // Set the sampler texture unit to 0
   glUniform1i ( userData->samplerLoc, 0 );

   glDrawElements ( GL_TRIANGLES, userData->numIndices, 
                    GL_UNSIGNED_SHORT, 0 );
}

///
// Cleanup
//
void stcShutDown ( ESContext *esContext )
{
   STCUserData *userData = esContext->userData;

   // Delete texture object
   glDeleteTextures ( 1, &userData->textureId );

   // Delete program object
   glDeleteProgram ( userData->programObject );

   // Delete vertex buffer objects
   glDeleteBuffers ( 3, userData->vboIds );
}
