//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

// Simple_VertexShader.c
//
//    This is a simple example that draws a rotating cube in perspective
//    using a vertex shader to transform the object
//

#include "Simple_VertexShader.h"
#include <stdlib.h>

///
// Initialize the shader and program object
//
int svsInit ( ESContext *esContext )
{
   SVSUserData *userData = esContext->userData;
   GLbyte vShaderStr[] =  
      "uniform mat4 u_mvpMatrix;                   \n"
      "attribute vec4 a_position;                  \n"
      "void main()                                 \n"
      "{                                           \n"
      "   gl_Position = u_mvpMatrix * a_position;  \n"
      "}                                           \n";
   
   // TODO(alokp): Shaders containing "precision" do not compile.
   GLbyte fShaderStr[] =  
      "//precision mediump float;                            \n"
      "void main()                                         \n"
      "{                                                   \n"
      "  gl_FragColor = vec4( 1.0, 0.0, 0.0, 1.0 );        \n"
      "}                                                   \n";

   // Load the shaders and get a linked program object
   userData->programObject = esLoadProgram ( vShaderStr, fShaderStr );

   // Get the attribute locations
   userData->positionLoc = glGetAttribLocation ( userData->programObject, "a_position" );

   // Get the uniform locations
   userData->mvpLoc = glGetUniformLocation( userData->programObject, "u_mvpMatrix" );
   
   // Generate the vertex data
   userData->numIndices = esGenCube( 1.0, &userData->vertices,
                                     NULL, NULL, &userData->indices );
   
   // Starting rotation angle for the cube
   userData->angle = 45.0f;

   glClearColor ( 0.0f, 0.0f, 0.0f, 0.0f );
   return TRUE;
}

///
// Update MVP matrix based on time
//
void svsUpdate ( ESContext *esContext, float deltaTime )
{
   SVSUserData *userData = (SVSUserData*) esContext->userData;
   ESMatrix perspective;
   ESMatrix modelview;
   float    aspect;
   
   // Compute a rotation angle based on time to rotate the cube
   userData->angle += ( deltaTime * 40.0f );
   if( userData->angle >= 360.0f )
      userData->angle -= 360.0f;

   // Compute the window aspect ratio
   aspect = (GLfloat) esContext->width / (GLfloat) esContext->height;
   
   // Generate a perspective matrix with a 60 degree FOV
   esMatrixLoadIdentity( &perspective );
   esPerspective( &perspective, 60.0f, aspect, 1.0f, 20.0f );

   // Generate a model view matrix to rotate/translate the cube
   esMatrixLoadIdentity( &modelview );

   // Translate away from the viewer
   esTranslate( &modelview, 0.0, 0.0, -2.0 );

   // Rotate the cube
   esRotate( &modelview, userData->angle, 1.0, 0.0, 1.0 );
   
   // Compute the final MVP by multiplying the 
   // modevleiw and perspective matrices together
   esMatrixMultiply( &userData->mvpMatrix, &modelview, &perspective );
}

///
// Draw a triangle using the shader pair created in Init()
//
void svsDraw ( ESContext *esContext )
{
   SVSUserData *userData = esContext->userData;
   
   // Set the viewport
   glViewport ( 0, 0, esContext->width, esContext->height );
   
   
   // Clear the color buffer
   glClear ( GL_COLOR_BUFFER_BIT );

   // Use the program object
   glUseProgram ( userData->programObject );

   // Load the vertex position
   glVertexAttribPointer ( userData->positionLoc, 3, GL_FLOAT, 
                           GL_FALSE, 3 * sizeof(GLfloat), userData->vertices );
   
   glEnableVertexAttribArray ( userData->positionLoc );
   
   
   // Load the MVP matrix
   glUniformMatrix4fv( userData->mvpLoc, 1, GL_FALSE, (GLfloat*) &userData->mvpMatrix.m[0][0] );
   
   // Draw the cube
   glDrawElements ( GL_TRIANGLES, userData->numIndices, GL_UNSIGNED_INT, userData->indices );

   // TODO(alokp): glFlush should not be necessary with SwapBuffers.
   glFlush();
}

///
// Cleanup
//
void svsShutDown ( ESContext *esContext )
{
   SVSUserData *userData = esContext->userData;

   if ( userData->vertices != NULL )
   {
      free ( userData->vertices );
   }

   if ( userData->indices != NULL )
   {
      free ( userData->indices );
   }

   // Delete program object
   glDeleteProgram ( userData->programObject );
}
