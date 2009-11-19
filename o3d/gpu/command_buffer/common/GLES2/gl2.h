#ifndef __gl2_h_
#define __gl2_h_

/* $Revision: 8784 $ on $Date:: 2009-09-02 09:49:17 -0700 #$ */

/*
 * This document is licensed under the SGI Free Software B License Version
 * 2.0. For details, see http://oss.sgi.com/projects/FreeB/ .
 */

#include <GLES2/gl2types.h>

#ifdef __cplusplus
#include "gpu/command_buffer/client/gles2_lib.h"
#define GLES2_GET_FUN(name) gles2::GetGLContext()->name
#else
#define GLES2_GET_FUN(name) GLES2 ## name
#endif

/*-------------------------------------------------------------------------
 * GL core functions.
 *-----------------------------------------------------------------------*/
#undef GL_APICALL
#define GL_APICALL
#undef GL_APIENTRY
#define GL_APIENTRY

#define glActiveTexture GLES2_GET_FUN(ActiveTexture)
#define glAttachShader GLES2_GET_FUN(AttachShader)
#define glBindAttribLocation GLES2_GET_FUN(BindAttribLocation)
#define glBindBuffer GLES2_GET_FUN(BindBuffer)
#define glBindFramebuffer GLES2_GET_FUN(BindFramebuffer)
#define glBindRenderbuffer GLES2_GET_FUN(BindRenderbuffer)
#define glBindTexture GLES2_GET_FUN(BindTexture)
#define glBlendColor GLES2_GET_FUN(BlendColor)
#define glBlendEquation GLES2_GET_FUN(BlendEquation)
#define glBlendEquationSeparate GLES2_GET_FUN(BlendEquationSeparate)
#define glBlendFunc GLES2_GET_FUN(BlendFunc)
#define glBlendFuncSeparate GLES2_GET_FUN(BlendFuncSeparate)
#define glBufferData GLES2_GET_FUN(BufferData)
#define glBufferSubData GLES2_GET_FUN(BufferSubData)
#define glCheckFramebufferStatus GLES2_GET_FUN(CheckFramebufferStatus)
#define glClear GLES2_GET_FUN(Clear)
#define glClearColor GLES2_GET_FUN(ClearColor)
#define glClearDepthf GLES2_GET_FUN(ClearDepthf)
#define glClearStencil GLES2_GET_FUN(ClearStencil)
#define glColorMask GLES2_GET_FUN(ColorMask)
#define glCompileShader GLES2_GET_FUN(CompileShader)
#define glCompressedTexImage2D GLES2_GET_FUN(CompressedTexImage2D)
#define glCompressedTexSubImage2D GLES2_GET_FUN(CompressedTexSubImage2D)
#define glCopyTexImage2D GLES2_GET_FUN(CopyTexImage2D)
#define glCopyTexSubImage2D GLES2_GET_FUN(CopyTexSubImage2D)
#define glCreateProgram GLES2_GET_FUN(CreateProgram)
#define glCreateShader GLES2_GET_FUN(CreateShader)
#define glCullFace GLES2_GET_FUN(CullFace)
#define glDeleteBuffers GLES2_GET_FUN(DeleteBuffers)
#define glDeleteFramebuffers GLES2_GET_FUN(DeleteFramebuffers)
#define glDeleteProgram GLES2_GET_FUN(DeleteProgram)
#define glDeleteRenderbuffers GLES2_GET_FUN(DeleteRenderbuffers)
#define glDeleteShader GLES2_GET_FUN(DeleteShader)
#define glDeleteTextures GLES2_GET_FUN(DeleteTextures)
#define glDepthFunc GLES2_GET_FUN(DepthFunc)
#define glDepthMask GLES2_GET_FUN(DepthMask)
#define glDepthRangef GLES2_GET_FUN(DepthRangef)
#define glDetachShader GLES2_GET_FUN(DetachShader)
#define glDisable GLES2_GET_FUN(Disable)
#define glDisableVertexAttribArray GLES2_GET_FUN(DisableVertexAttribArray)
#define glDrawArrays GLES2_GET_FUN(DrawArrays)
#define glDrawElements GLES2_GET_FUN(DrawElements)
#define glEnable GLES2_GET_FUN(Enable)
#define glEnableVertexAttribArray GLES2_GET_FUN(EnableVertexAttribArray)
#define glFinish GLES2_GET_FUN(Finish)
#define glFlush GLES2_GET_FUN(Flush)
#define glFramebufferRenderbuffer GLES2_GET_FUN(FramebufferRenderbuffer)
#define glFramebufferTexture2D GLES2_GET_FUN(FramebufferTexture2D)
#define glFrontFace GLES2_GET_FUN(FrontFace)
#define glGenBuffers GLES2_GET_FUN(GenBuffers)
#define glGenerateMipmap GLES2_GET_FUN(GenerateMipmap)
#define glGenFramebuffers GLES2_GET_FUN(GenFramebuffers)
#define glGenRenderbuffers GLES2_GET_FUN(GenRenderbuffers)
#define glGenTextures GLES2_GET_FUN(GenTextures)
#define glGetActiveAttrib GLES2_GET_FUN(GetActiveAttrib)
#define glGetActiveUniform GLES2_GET_FUN(GetActiveUniform)
#define glGetAttachedShaders GLES2_GET_FUN(GetAttachedShaders)
#define glGetAttribLocation GLES2_GET_FUN(GetAttribLocation)
#define glGetBooleanv GLES2_GET_FUN(GetBooleanv)
#define glGetBufferParameteriv GLES2_GET_FUN(GetBufferParameteriv)
#define glGetError GLES2_GET_FUN(GetError)
#define glGetFloatv GLES2_GET_FUN(GetFloatv)
#define glGetFramebufferAttachmentParameteriv GLES2_GET_FUN(GetFramebufferAttachmentParameteriv)
#define glGetIntegerv GLES2_GET_FUN(GetIntegerv)
#define glGetProgramiv GLES2_GET_FUN(GetProgramiv)
#define glGetProgramInfoLog GLES2_GET_FUN(GetProgramInfoLog)
#define glGetRenderbufferParameteriv GLES2_GET_FUN(GetRenderbufferParameteriv)
#define glGetShaderiv GLES2_GET_FUN(GetShaderiv)
#define glGetShaderInfoLog GLES2_GET_FUN(GetShaderInfoLog)
#define glGetShaderPrecisionFormat GLES2_GET_FUN(GetShaderPrecisionFormat)
#define glGetShaderSource GLES2_GET_FUN(GetShaderSource)
#define glGetString GLES2_GET_FUN(GetString)
#define glGetTexParameterfv GLES2_GET_FUN(GetTexParameterfv)
#define glGetTexParameteriv GLES2_GET_FUN(GetTexParameteriv)
#define glGetUniformfv GLES2_GET_FUN(GetUniformfv)
#define glGetUniformiv GLES2_GET_FUN(GetUniformiv)
#define glGetUniformLocation GLES2_GET_FUN(GetUniformLocation)
#define glGetVertexAttribfv GLES2_GET_FUN(GetVertexAttribfv)
#define glGetVertexAttribiv GLES2_GET_FUN(GetVertexAttribiv)
#define glGetVertexAttribPointerv GLES2_GET_FUN(GetVertexAttribPointerv)
#define glHint GLES2_GET_FUN(Hint)
#define glIsBuffer GLES2_GET_FUN(IsBuffer)
#define glIsEnabled GLES2_GET_FUN(IsEnabled)
#define glIsFramebuffer GLES2_GET_FUN(IsFramebuffer)
#define glIsProgram GLES2_GET_FUN(IsProgram)
#define glIsRenderbuffer GLES2_GET_FUN(IsRenderbuffer)
#define glIsShader GLES2_GET_FUN(IsShader)
#define glIsTexture GLES2_GET_FUN(IsTexture)
#define glLineWidth GLES2_GET_FUN(LineWidth)
#define glLinkProgram GLES2_GET_FUN(LinkProgram)
#define glPixelStorei GLES2_GET_FUN(PixelStorei)
#define glPolygonOffset GLES2_GET_FUN(PolygonOffset)
#define glReadPixels GLES2_GET_FUN(ReadPixels)
#define glReleaseShaderCompiler GLES2_GET_FUN(ReleaseShaderCompiler)
#define glRenderbufferStorage GLES2_GET_FUN(RenderbufferStorage)
#define glSampleCoverage GLES2_GET_FUN(SampleCoverage)
#define glScissor GLES2_GET_FUN(Scissor)
#define glShaderBinary GLES2_GET_FUN(ShaderBinary)
#define glShaderSource GLES2_GET_FUN(ShaderSource)
#define glStencilFunc GLES2_GET_FUN(StencilFunc)
#define glStencilFuncSeparate GLES2_GET_FUN(StencilFuncSeparate)
#define glStencilMask GLES2_GET_FUN(StencilMask)
#define glStencilMaskSeparate GLES2_GET_FUN(StencilMaskSeparate)
#define glStencilOp GLES2_GET_FUN(StencilOp)
#define glStencilOpSeparate GLES2_GET_FUN(StencilOpSeparate)
#define glTexImage2D GLES2_GET_FUN(TexImage2D)
#define glTexParameterf GLES2_GET_FUN(TexParameterf)
#define glTexParameterfv GLES2_GET_FUN(TexParameterfv)
#define glTexParameteri GLES2_GET_FUN(TexParameteri)
#define glTexParameteriv GLES2_GET_FUN(TexParameteriv)
#define glTexSubImage2D GLES2_GET_FUN(TexSubImage2D)
#define glUniform1f GLES2_GET_FUN(Uniform1f)
#define glUniform1fv GLES2_GET_FUN(Uniform1fv)
#define glUniform1i GLES2_GET_FUN(Uniform1i)
#define glUniform1iv GLES2_GET_FUN(Uniform1iv)
#define glUniform2f GLES2_GET_FUN(Uniform2f)
#define glUniform2fv GLES2_GET_FUN(Uniform2fv)
#define glUniform2i GLES2_GET_FUN(Uniform2i)
#define glUniform2iv GLES2_GET_FUN(Uniform2iv)
#define glUniform3f GLES2_GET_FUN(Uniform3f)
#define glUniform3fv GLES2_GET_FUN(Uniform3fv)
#define glUniform3i GLES2_GET_FUN(Uniform3i)
#define glUniform3iv GLES2_GET_FUN(Uniform3iv)
#define glUniform4f GLES2_GET_FUN(Uniform4f)
#define glUniform4fv GLES2_GET_FUN(Uniform4fv)
#define glUniform4i GLES2_GET_FUN(Uniform4i)
#define glUniform4iv GLES2_GET_FUN(Uniform4iv)
#define glUniformMatrix2fv GLES2_GET_FUN(UniformMatrix2fv)
#define glUniformMatrix3fv GLES2_GET_FUN(UniformMatrix3fv)
#define glUniformMatrix4fv GLES2_GET_FUN(UniformMatrix4fv)
#define glUseProgram GLES2_GET_FUN(UseProgram)
#define glValidateProgram GLES2_GET_FUN(ValidateProgram)
#define glVertexAttrib1f GLES2_GET_FUN(VertexAttrib1f)
#define glVertexAttrib1fv GLES2_GET_FUN(VertexAttrib1fv)
#define glVertexAttrib2f GLES2_GET_FUN(VertexAttrib2f)
#define glVertexAttrib2fv GLES2_GET_FUN(VertexAttrib2fv)
#define glVertexAttrib3f GLES2_GET_FUN(VertexAttrib3f)
#define glVertexAttrib3fv GLES2_GET_FUN(VertexAttrib3fv)
#define glVertexAttrib4f GLES2_GET_FUN(VertexAttrib4f)
#define glVertexAttrib4fv GLES2_GET_FUN(VertexAttrib4fv)
#define glVertexAttribPointer GLES2_GET_FUN(VertexAttribPointer)
#define glViewport GLES2_GET_FUN(Viewport)

#ifndef __cplusplus

GL_APICALL void         GL_APIENTRY glActiveTexture (GLenum texture);
GL_APICALL void         GL_APIENTRY glAttachShader (GLuint program, GLuint shader);
GL_APICALL void         GL_APIENTRY glBindAttribLocation (GLuint program, GLuint index, const char* name);
GL_APICALL void         GL_APIENTRY glBindBuffer (GLenum target, GLuint buffer);
GL_APICALL void         GL_APIENTRY glBindFramebuffer (GLenum target, GLuint framebuffer);
GL_APICALL void         GL_APIENTRY glBindRenderbuffer (GLenum target, GLuint renderbuffer);
GL_APICALL void         GL_APIENTRY glBindTexture (GLenum target, GLuint texture);
GL_APICALL void         GL_APIENTRY glBlendColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
GL_APICALL void         GL_APIENTRY glBlendEquation ( GLenum mode );
GL_APICALL void         GL_APIENTRY glBlendEquationSeparate (GLenum modeRGB, GLenum modeAlpha);
GL_APICALL void         GL_APIENTRY glBlendFunc (GLenum sfactor, GLenum dfactor);
GL_APICALL void         GL_APIENTRY glBlendFuncSeparate (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
GL_APICALL void         GL_APIENTRY glBufferData (GLenum target, GLsizeiptr size, const void* data, GLenum usage);
GL_APICALL void         GL_APIENTRY glBufferSubData (GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
GL_APICALL GLenum       GL_APIENTRY glCheckFramebufferStatus (GLenum target);
GL_APICALL void         GL_APIENTRY glClear (GLbitfield mask);
GL_APICALL void         GL_APIENTRY glClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
GL_APICALL void         GL_APIENTRY glClearDepthf (GLclampf depth);
GL_APICALL void         GL_APIENTRY glClearStencil (GLint s);
GL_APICALL void         GL_APIENTRY glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
GL_APICALL void         GL_APIENTRY glCompileShader (GLuint shader);
GL_APICALL void         GL_APIENTRY glCompressedTexImage2D (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data);
GL_APICALL void         GL_APIENTRY glCompressedTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data);
GL_APICALL void         GL_APIENTRY glCopyTexImage2D (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
GL_APICALL void         GL_APIENTRY glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GL_APICALL GLuint       GL_APIENTRY glCreateProgram (void);
GL_APICALL GLuint       GL_APIENTRY glCreateShader (GLenum type);
GL_APICALL void         GL_APIENTRY glCullFace (GLenum mode);
GL_APICALL void         GL_APIENTRY glDeleteBuffers (GLsizei n, const GLuint* buffers);
GL_APICALL void         GL_APIENTRY glDeleteFramebuffers (GLsizei n, const GLuint* framebuffers);
GL_APICALL void         GL_APIENTRY glDeleteProgram (GLuint program);
GL_APICALL void         GL_APIENTRY glDeleteRenderbuffers (GLsizei n, const GLuint* renderbuffers);
GL_APICALL void         GL_APIENTRY glDeleteShader (GLuint shader);
GL_APICALL void         GL_APIENTRY glDeleteTextures (GLsizei n, const GLuint* textures);
GL_APICALL void         GL_APIENTRY glDepthFunc (GLenum func);
GL_APICALL void         GL_APIENTRY glDepthMask (GLboolean flag);
GL_APICALL void         GL_APIENTRY glDepthRangef (GLclampf zNear, GLclampf zFar);
GL_APICALL void         GL_APIENTRY glDetachShader (GLuint program, GLuint shader);
GL_APICALL void         GL_APIENTRY glDisable (GLenum cap);
GL_APICALL void         GL_APIENTRY glDisableVertexAttribArray (GLuint index);
GL_APICALL void         GL_APIENTRY glDrawArrays (GLenum mode, GLint first, GLsizei count);
GL_APICALL void         GL_APIENTRY glDrawElements (GLenum mode, GLsizei count, GLenum type, const void* indices);
GL_APICALL void         GL_APIENTRY glEnable (GLenum cap);
GL_APICALL void         GL_APIENTRY glEnableVertexAttribArray (GLuint index);
GL_APICALL void         GL_APIENTRY glFinish (void);
GL_APICALL void         GL_APIENTRY glFlush (void);
GL_APICALL void         GL_APIENTRY glFramebufferRenderbuffer (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
GL_APICALL void         GL_APIENTRY glFramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
GL_APICALL void         GL_APIENTRY glFrontFace (GLenum mode);
GL_APICALL void         GL_APIENTRY glGenBuffers (GLsizei n, GLuint* buffers);
GL_APICALL void         GL_APIENTRY glGenerateMipmap (GLenum target);
GL_APICALL void         GL_APIENTRY glGenFramebuffers (GLsizei n, GLuint* framebuffers);
GL_APICALL void         GL_APIENTRY glGenRenderbuffers (GLsizei n, GLuint* renderbuffers);
GL_APICALL void         GL_APIENTRY glGenTextures (GLsizei n, GLuint* textures);
GL_APICALL void         GL_APIENTRY glGetActiveAttrib (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name);
GL_APICALL void         GL_APIENTRY glGetActiveUniform (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name);
GL_APICALL void         GL_APIENTRY glGetAttachedShaders (GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders);
GL_APICALL int          GL_APIENTRY glGetAttribLocation (GLuint program, const char* name);
GL_APICALL void         GL_APIENTRY glGetBooleanv (GLenum pname, GLboolean* params);
GL_APICALL void         GL_APIENTRY glGetBufferParameteriv (GLenum target, GLenum pname, GLint* params);
GL_APICALL GLenum       GL_APIENTRY glGetError (void);
GL_APICALL void         GL_APIENTRY glGetFloatv (GLenum pname, GLfloat* params);
GL_APICALL void         GL_APIENTRY glGetFramebufferAttachmentParameteriv (GLenum target, GLenum attachment, GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetIntegerv (GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetProgramiv (GLuint program, GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetProgramInfoLog (GLuint program, GLsizei bufsize, GLsizei* length, char* infolog);
GL_APICALL void         GL_APIENTRY glGetRenderbufferParameteriv (GLenum target, GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetShaderiv (GLuint shader, GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetShaderInfoLog (GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog);
GL_APICALL void         GL_APIENTRY glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);
GL_APICALL void         GL_APIENTRY glGetShaderSource (GLuint shader, GLsizei bufsize, GLsizei* length, char* source);
GL_APICALL const GLubyte* GL_APIENTRY glGetString (GLenum name);
GL_APICALL void         GL_APIENTRY glGetTexParameterfv (GLenum target, GLenum pname, GLfloat* params);
GL_APICALL void         GL_APIENTRY glGetTexParameteriv (GLenum target, GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetUniformfv (GLuint program, GLint location, GLfloat* params);
GL_APICALL void         GL_APIENTRY glGetUniformiv (GLuint program, GLint location, GLint* params);
GL_APICALL int          GL_APIENTRY glGetUniformLocation (GLuint program, const char* name);
GL_APICALL void         GL_APIENTRY glGetVertexAttribfv (GLuint index, GLenum pname, GLfloat* params);
GL_APICALL void         GL_APIENTRY glGetVertexAttribiv (GLuint index, GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetVertexAttribPointerv (GLuint index, GLenum pname, void** pointer);
GL_APICALL void         GL_APIENTRY glHint (GLenum target, GLenum mode);
GL_APICALL GLboolean    GL_APIENTRY glIsBuffer (GLuint buffer);
GL_APICALL GLboolean    GL_APIENTRY glIsEnabled (GLenum cap);
GL_APICALL GLboolean    GL_APIENTRY glIsFramebuffer (GLuint framebuffer);
GL_APICALL GLboolean    GL_APIENTRY glIsProgram (GLuint program);
GL_APICALL GLboolean    GL_APIENTRY glIsRenderbuffer (GLuint renderbuffer);
GL_APICALL GLboolean    GL_APIENTRY glIsShader (GLuint shader);
GL_APICALL GLboolean    GL_APIENTRY glIsTexture (GLuint texture);
GL_APICALL void         GL_APIENTRY glLineWidth (GLfloat width);
GL_APICALL void         GL_APIENTRY glLinkProgram (GLuint program);
GL_APICALL void         GL_APIENTRY glPixelStorei (GLenum pname, GLint param);
GL_APICALL void         GL_APIENTRY glPolygonOffset (GLfloat factor, GLfloat units);
GL_APICALL void         GL_APIENTRY glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
GL_APICALL void         GL_APIENTRY glReleaseShaderCompiler (void);
GL_APICALL void         GL_APIENTRY glRenderbufferStorage (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
GL_APICALL void         GL_APIENTRY glSampleCoverage (GLclampf value, GLboolean invert);
GL_APICALL void         GL_APIENTRY glScissor (GLint x, GLint y, GLsizei width, GLsizei height);
GL_APICALL void         GL_APIENTRY glShaderBinary (GLsizei n, const GLuint* shaders, GLenum binaryformat, const void* binary, GLsizei length);
GL_APICALL void         GL_APIENTRY glShaderSource (GLuint shader, GLsizei count, const char** string, const GLint* length);
GL_APICALL void         GL_APIENTRY glStencilFunc (GLenum func, GLint ref, GLuint mask);
GL_APICALL void         GL_APIENTRY glStencilFuncSeparate (GLenum face, GLenum func, GLint ref, GLuint mask);
GL_APICALL void         GL_APIENTRY glStencilMask (GLuint mask);
GL_APICALL void         GL_APIENTRY glStencilMaskSeparate (GLenum face, GLuint mask);
GL_APICALL void         GL_APIENTRY glStencilOp (GLenum fail, GLenum zfail, GLenum zpass);
GL_APICALL void         GL_APIENTRY glStencilOpSeparate (GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
GL_APICALL void         GL_APIENTRY glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
GL_APICALL void         GL_APIENTRY glTexParameterf (GLenum target, GLenum pname, GLfloat param);
GL_APICALL void         GL_APIENTRY glTexParameterfv (GLenum target, GLenum pname, const GLfloat* params);
GL_APICALL void         GL_APIENTRY glTexParameteri (GLenum target, GLenum pname, GLint param);
GL_APICALL void         GL_APIENTRY glTexParameteriv (GLenum target, GLenum pname, const GLint* params);
GL_APICALL void         GL_APIENTRY glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
GL_APICALL void         GL_APIENTRY glUniform1f (GLint location, GLfloat x);
GL_APICALL void         GL_APIENTRY glUniform1fv (GLint location, GLsizei count, const GLfloat* v);
GL_APICALL void         GL_APIENTRY glUniform1i (GLint location, GLint x);
GL_APICALL void         GL_APIENTRY glUniform1iv (GLint location, GLsizei count, const GLint* v);
GL_APICALL void         GL_APIENTRY glUniform2f (GLint location, GLfloat x, GLfloat y);
GL_APICALL void         GL_APIENTRY glUniform2fv (GLint location, GLsizei count, const GLfloat* v);
GL_APICALL void         GL_APIENTRY glUniform2i (GLint location, GLint x, GLint y);
GL_APICALL void         GL_APIENTRY glUniform2iv (GLint location, GLsizei count, const GLint* v);
GL_APICALL void         GL_APIENTRY glUniform3f (GLint location, GLfloat x, GLfloat y, GLfloat z);
GL_APICALL void         GL_APIENTRY glUniform3fv (GLint location, GLsizei count, const GLfloat* v);
GL_APICALL void         GL_APIENTRY glUniform3i (GLint location, GLint x, GLint y, GLint z);
GL_APICALL void         GL_APIENTRY glUniform3iv (GLint location, GLsizei count, const GLint* v);
GL_APICALL void         GL_APIENTRY glUniform4f (GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
GL_APICALL void         GL_APIENTRY glUniform4fv (GLint location, GLsizei count, const GLfloat* v);
GL_APICALL void         GL_APIENTRY glUniform4i (GLint location, GLint x, GLint y, GLint z, GLint w);
GL_APICALL void         GL_APIENTRY glUniform4iv (GLint location, GLsizei count, const GLint* v);
GL_APICALL void         GL_APIENTRY glUniformMatrix2fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GL_APICALL void         GL_APIENTRY glUniformMatrix3fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GL_APICALL void         GL_APIENTRY glUniformMatrix4fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
GL_APICALL void         GL_APIENTRY glUseProgram (GLuint program);
GL_APICALL void         GL_APIENTRY glValidateProgram (GLuint program);
GL_APICALL void         GL_APIENTRY glVertexAttrib1f (GLuint indx, GLfloat x);
GL_APICALL void         GL_APIENTRY glVertexAttrib1fv (GLuint indx, const GLfloat* values);
GL_APICALL void         GL_APIENTRY glVertexAttrib2f (GLuint indx, GLfloat x, GLfloat y);
GL_APICALL void         GL_APIENTRY glVertexAttrib2fv (GLuint indx, const GLfloat* values);
GL_APICALL void         GL_APIENTRY glVertexAttrib3f (GLuint indx, GLfloat x, GLfloat y, GLfloat z);
GL_APICALL void         GL_APIENTRY glVertexAttrib3fv (GLuint indx, const GLfloat* values);
GL_APICALL void         GL_APIENTRY glVertexAttrib4f (GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
GL_APICALL void         GL_APIENTRY glVertexAttrib4fv (GLuint indx, const GLfloat* values);
GL_APICALL void         GL_APIENTRY glVertexAttribPointer (GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* ptr);
GL_APICALL void         GL_APIENTRY glViewport (GLint x, GLint y, GLsizei width, GLsizei height);

#endif  // __cplusplus

#endif /* __gl2_h_ */

