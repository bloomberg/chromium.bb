//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

// Hello_Triangle.c
//
//    This is a simple example that draws a single triangle with
//    a minimal vertex/fragment shader.  The purpose of this 
//    example is to demonstrate the basic concepts of 
//    OpenGL ES 2.0 rendering.

#include "Hello_Triangle.h"

#include <stdlib.h>

///
// Initialize the shader and program object
//
int htInit ( ESContext *esContext )
{
   HTUserData *userData = esContext->userData;

   GLbyte vShaderStr[] =  
      "attribute vec4 vPosition;    \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = vPosition;  \n"
      "}                            \n";
   
   // TODO(alokp): Shaders containing "precision" do not compile.
   GLbyte fShaderStr[] =  
      "//precision mediump float;                      \n"
      "void main()                                   \n"
      "{                                             \n"
      "   gl_FragColor = vec4 ( 1.0, 0.0, 0.0, 1.0 );\n"
      "}                                             \n";

   GLfloat vVertices[] = {  0.0f,  0.5f, 0.0f, 
                           -0.5f, -0.5f, 0.0f,
                            0.5f, -0.5f, 0.0f };

   userData->programObject = esLoadProgram ( vShaderStr, fShaderStr );
   if ( userData->programObject == 0 ) return FALSE;

   // Bind vPosition to attribute 0   
   glBindAttribLocation ( userData->programObject, 0, "vPosition" );

   glGenBuffers ( 1, &userData->vbo );
   glBindBuffer ( GL_ARRAY_BUFFER, userData->vbo );
   glBufferData ( GL_ARRAY_BUFFER, sizeof(vVertices), NULL, GL_STATIC_DRAW );
   glBufferSubData ( GL_ARRAY_BUFFER, 0, sizeof(vVertices), vVertices );

   glClearColor ( 0.0f, 0.0f, 0.0f, 0.0f );
   return TRUE;
}

///
// Draw a triangle using the shader pair created in Init()
//
void htDraw ( ESContext *esContext )
{
   HTUserData *userData = esContext->userData;

   // Set the viewport
   glViewport ( 0, 0, esContext->width, esContext->height );
   
   // Clear the color buffer
   glClear ( GL_COLOR_BUFFER_BIT );

   // Use the program object
   glUseProgram ( userData->programObject );

   // Load the vertex data
   glBindBuffer ( GL_ARRAY_BUFFER, userData->vbo );
   glEnableVertexAttribArray ( 0 );
   glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 );

   glDrawArrays ( GL_TRIANGLES, 0, 3 );

   // Nothing is drawn or application crashes without glFlush.
   // TODO(alokp): glFlush should not be necessary with SwapBuffers().
   glFlush();
}

///
// Cleanup
//
void htShutDown ( ESContext *esContext )
{
   HTUserData *userData = esContext->userData;

   // Delete program object
   if ( userData->programObject != 0 )
   {
      glDeleteProgram ( userData->programObject );
      userData->programObject = 0;
   }
   if ( userData->vbo != 0 )
   {
      glDeleteBuffers ( 1, &userData->vbo );
      userData->vbo = 0;
   }
}
