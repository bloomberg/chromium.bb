#!/usr/bin/python
#
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""code generator for GL command buffers."""

import os
import os.path
import sys
import re
from optparse import OptionParser

_SIZE_OF_UINT32 = 4
_SIZE_OF_COMMAND_HEADER = 4
_FIRST_SPECIFIC_COMMAND_ID = 1024

_LICENSE = """
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"""

# This string is copied directly out of the gl2.h file from GLES2.0
# the reasons it is copied out instead of parsed directly are
#
# 1) Because order is important. The command IDs need to stay constant forever
#    so if you add a new command it needs to be added to the bottom of the list.
#
# 2) So we can add more commands easily that are unrelated to GLES2.0 but still
#    needed for GLES2.0 command buffers.
#
# Edits:
#
# *) Any argument that is a resourceID has been changed to GLresourceID.
#    (not pointer arguments)
#
_GL_FUNCTIONS = """
GL_APICALL void         GL_APIENTRY glActiveTexture (GLenum texture);
GL_APICALL void         GL_APIENTRY glAttachShader (GLResourceId program, GLResourceId shader);
GL_APICALL void         GL_APIENTRY glBindAttribLocation (GLResourceId program, GLuint index, const char* name);
GL_APICALL void         GL_APIENTRY glBindBuffer (GLenum target, GLResourceId buffer);
GL_APICALL void         GL_APIENTRY glBindFramebuffer (GLenum target, GLResourceId framebuffer);
GL_APICALL void         GL_APIENTRY glBindRenderbuffer (GLenum target, GLResourceId renderbuffer);
GL_APICALL void         GL_APIENTRY glBindTexture (GLenum target, GLResourceId texture);
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
GL_APICALL void         GL_APIENTRY glCompileShader (GLResourceId shader);
GL_APICALL void         GL_APIENTRY glCompressedTexImage2D (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data);
GL_APICALL void         GL_APIENTRY glCompressedTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data);
GL_APICALL void         GL_APIENTRY glCopyTexImage2D (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
GL_APICALL void         GL_APIENTRY glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GL_APICALL GLuint       GL_APIENTRY glCreateProgram (void);
GL_APICALL GLuint       GL_APIENTRY glCreateShader (GLenum type);
GL_APICALL void         GL_APIENTRY glCullFace (GLenum mode);
GL_APICALL void         GL_APIENTRY glDeleteBuffers (GLsizei n, const GLuint* buffers);
GL_APICALL void         GL_APIENTRY glDeleteFramebuffers (GLsizei n, const GLuint* framebuffers);
GL_APICALL void         GL_APIENTRY glDeleteProgram (GLResourceId program);
GL_APICALL void         GL_APIENTRY glDeleteRenderbuffers (GLsizei n, const GLuint* renderbuffers);
GL_APICALL void         GL_APIENTRY glDeleteShader (GLResourceId shader);
GL_APICALL void         GL_APIENTRY glDeleteTextures (GLsizei n, const GLuint* textures);
GL_APICALL void         GL_APIENTRY glDepthFunc (GLenum func);
GL_APICALL void         GL_APIENTRY glDepthMask (GLboolean flag);
GL_APICALL void         GL_APIENTRY glDepthRangef (GLclampf zNear, GLclampf zFar);
GL_APICALL void         GL_APIENTRY glDetachShader (GLResourceId program, GLResourceId shader);
GL_APICALL void         GL_APIENTRY glDisable (GLenum cap);
GL_APICALL void         GL_APIENTRY glDisableVertexAttribArray (GLuint index);
GL_APICALL void         GL_APIENTRY glDrawArrays (GLenum mode, GLint first, GLsizei count);
GL_APICALL void         GL_APIENTRY glDrawElements (GLenum mode, GLsizei count, GLenum type, const void* indices);
GL_APICALL void         GL_APIENTRY glEnable (GLenum cap);
GL_APICALL void         GL_APIENTRY glEnableVertexAttribArray (GLuint index);
GL_APICALL void         GL_APIENTRY glFinish (void);
GL_APICALL void         GL_APIENTRY glFlush (void);
GL_APICALL void         GL_APIENTRY glFramebufferRenderbuffer (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLResourceId renderbuffer);
GL_APICALL void         GL_APIENTRY glFramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget, GLResourceId texture, GLint level);
GL_APICALL void         GL_APIENTRY glFrontFace (GLenum mode);
GL_APICALL void         GL_APIENTRY glGenBuffers (GLsizei n, GLuint* buffers);
GL_APICALL void         GL_APIENTRY glGenerateMipmap (GLenum target);
GL_APICALL void         GL_APIENTRY glGenFramebuffers (GLsizei n, GLuint* framebuffers);
GL_APICALL void         GL_APIENTRY glGenRenderbuffers (GLsizei n, GLuint* renderbuffers);
GL_APICALL void         GL_APIENTRY glGenTextures (GLsizei n, GLuint* textures);
GL_APICALL void         GL_APIENTRY glGetActiveAttrib (GLResourceId program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name);
GL_APICALL void         GL_APIENTRY glGetActiveUniform (GLResourceId program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name);
GL_APICALL void         GL_APIENTRY glGetAttachedShaders (GLResourceId program, GLsizei maxcount, GLsizei* count, GLuint* shaders);
GL_APICALL GLint        GL_APIENTRY glGetAttribLocation (GLResourceId program, const char* name);
GL_APICALL void         GL_APIENTRY glGetBooleanv (GLenum pname, GLboolean* params);
GL_APICALL void         GL_APIENTRY glGetBufferParameteriv (GLenum target, GLenum pname, GLint* params);
GL_APICALL GLenum       GL_APIENTRY glGetError (void);
GL_APICALL void         GL_APIENTRY glGetFloatv (GLenum pname, GLfloat* params);
GL_APICALL void         GL_APIENTRY glGetFramebufferAttachmentParameteriv (GLenum target, GLenum attachment, GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetIntegerv (GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetProgramiv (GLResourceId program, GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetProgramInfoLog (GLResourceId program, GLsizei bufsize, GLsizei* length, char* infolog);
GL_APICALL void         GL_APIENTRY glGetRenderbufferParameteriv (GLenum target, GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetShaderiv (GLResourceId shader, GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetShaderInfoLog (GLResourceId shader, GLsizei bufsize, GLsizei* length, char* infolog);
GL_APICALL void         GL_APIENTRY glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);
GL_APICALL void         GL_APIENTRY glGetShaderSource (GLResourceId shader, GLsizei bufsize, GLsizei* length, char* source);
GL_APICALL const GLubyte* GL_APIENTRY glGetString (GLenum name);
GL_APICALL void         GL_APIENTRY glGetTexParameterfv (GLenum target, GLenum pname, GLfloat* params);
GL_APICALL void         GL_APIENTRY glGetTexParameteriv (GLenum target, GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetUniformfv (GLResourceId program, GLint location, GLfloat* params);
GL_APICALL void         GL_APIENTRY glGetUniformiv (GLResourceId program, GLint location, GLint* params);
GL_APICALL GLint        GL_APIENTRY glGetUniformLocation (GLResourceId program, const char* name);
GL_APICALL void         GL_APIENTRY glGetVertexAttribfv (GLuint index, GLenum pname, GLfloat* params);
GL_APICALL void         GL_APIENTRY glGetVertexAttribiv (GLuint index, GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetVertexAttribPointerv (GLuint index, GLenum pname, void** pointer);
GL_APICALL void         GL_APIENTRY glHint (GLenum target, GLenum mode);
GL_APICALL GLboolean    GL_APIENTRY glIsBuffer (GLResourceId buffer);
GL_APICALL GLboolean    GL_APIENTRY glIsEnabled (GLenum cap);
GL_APICALL GLboolean    GL_APIENTRY glIsFramebuffer (GLResourceId framebuffer);
GL_APICALL GLboolean    GL_APIENTRY glIsProgram (GLResourceId program);
GL_APICALL GLboolean    GL_APIENTRY glIsRenderbuffer (GLResourceId renderbuffer);
GL_APICALL GLboolean    GL_APIENTRY glIsShader (GLResourceId shader);
GL_APICALL GLboolean    GL_APIENTRY glIsTexture (GLResourceId texture);
GL_APICALL void         GL_APIENTRY glLineWidth (GLfloat width);
GL_APICALL void         GL_APIENTRY glLinkProgram (GLResourceId program);
GL_APICALL void         GL_APIENTRY glPixelStorei (GLenum pname, GLint param);
GL_APICALL void         GL_APIENTRY glPolygonOffset (GLfloat factor, GLfloat units);
GL_APICALL void         GL_APIENTRY glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
GL_APICALL void         GL_APIENTRY glReleaseShaderCompiler (void);
GL_APICALL void         GL_APIENTRY glRenderbufferStorage (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
GL_APICALL void         GL_APIENTRY glSampleCoverage (GLclampf value, GLboolean invert);
GL_APICALL void         GL_APIENTRY glScissor (GLint x, GLint y, GLsizei width, GLsizei height);
GL_APICALL void         GL_APIENTRY glShaderBinary (GLsizei n, const GLResourceId* shaders, GLenum binaryformat, const void* binary, GLsizei length);
GL_APICALL void         GL_APIENTRY glShaderSource (GLResourceId shader, GLsizei count, const char** string, const GLint* length);
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
GL_APICALL void         GL_APIENTRY glUseProgram (GLResourceId program);
GL_APICALL void         GL_APIENTRY glValidateProgram (GLResourceId program);
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
// Non-GL commands.
GL_APICALL void         GL_APIENTRY glSwapBuffers (void);
"""

# This table specifies types and other special data for the commands that
# will be generated.
#
# type:        defines which handler will be used to generate code.
# DecoderFunc: defines which function to call in the decoder to execute the
#              corresponding GL command. If not specified the GL command will
#              be called directly.
# cmd_args:    The arguments to use for the command. This overrides generating
#              them based on the GL function arguments.
#              a NonImmediate type is a type that stays a pointer even in
#              and immediate version of acommand.
# immediate:   Whether or not to generate an immediate command for the GL
#              function. The default is if there is exactly 1 pointer argument
#              in the GL function an immediate command is generated.
# needs_size:  If true a data_size field is added to the command.
# data_type:   The type of data the command uses. For PUTn or PUT types.
# count:       The number of units per element. For PUTn or PUT types.

_FUNCTION_INFO = {
  'BindAttribLocation': {'type': 'GLchar'},
  'BindBuffer': {'DecoderFunc': 'DoBindBuffer'},
  'BindFramebuffer': {'DecoderFunc': 'glBindFramebufferEXT'},
  'BindRenderbuffer': {'DecoderFunc': 'glBindRenderbufferEXT'},
  'BufferData': {'type': 'Manual', 'immediate': True,},
  'BufferSubData': {'type': 'Data'},
  'CheckFramebufferStatus': {'DecoderFunc': 'glCheckFramebufferStatusEXT'},
  'ClearDepthf': {'DecoderFunc': 'glClearDepth'},
  'CompressedTexImage2D': {'type': 'Manual', 'immediate': True,},
  'CompressedTexSubImage2D': {'type': 'Data'},
  'CreateProgram': {'type': 'Create'},
  'CreateShader': {'type': 'Create'},
  'DeleteBuffers': {'type': 'DELn'},
  'DeleteFramebuffers': {'type': 'DELn'},
  'DeleteProgram': {'DecoderFunc': 'DoDeleteProgram'},
  'DeleteRenderbuffers': {'type': 'DELn'},
  'DeleteShader': {'DecoderFunc': 'DoDeleteShader'},
  'DeleteTextures': {'type': 'DELn'},
  'DepthRangef': {'DecoderFunc': 'glDepthRange'},
  'DrawElements': {
    'type': 'Manual',
    'cmd_args': 'GLenum mode, GLsizei count, GLenum type, GLuint index_offset',
  },
  'FramebufferRenderbuffer': {'DecoderFunc': 'glFramebufferRenderbufferEXT'},
  'FramebufferTexture2D': {'DecoderFunc': 'glFramebufferTexture2DEXT'},
  'GenerateMipmap': {'DecoderFunc': 'glGenerateMipmapEXT'},
  'GenBuffers': {'type': 'GENn'},
  'GenFramebuffers': {'type': 'GENn'},
  'GenRenderbuffers': {'type': 'GENn'},
  'GenTextures': {'type': 'GENn'},
  'GetActiveAttrib': {'type': 'Custom'},
  'GetActiveUniform': {'type': 'Custom'},
  'GetAttachedShaders': {'type': 'Custom'},
  'GetAttribLocation': {
    'type': 'HandWritten',
    'immediate': True,
    'needs_size': True,
    'cmd_args':
        'GLResourceId program, const char* name, NonImmediate GLint* location',
  },
  'GetBooleanv': {'type': 'GETn'},
  'GetBufferParameteriv': {'type': 'GETn'},
  'GetError': {'type': 'Is', 'DecoderFunc': 'GetGLError'},
  'GetFloatv': {'type': 'GETn'},
  'GetFramebufferAttachmentParameteriv': {
    'type': 'GETn',
    'DecoderFunc': 'glGetFramebufferAttachmentParameterivEXT',
  },
  'GetIntegerv': {'type': 'GETn'},
  'GetProgramiv': {'type': 'GETn'},
  'GetProgramInfoLog': {'type': 'STRn'},
  'GetRenderbufferParameteriv': {
    'type': 'GETn',
    'DecoderFunc': 'glGetRenderbufferParameterivEXT',
  },
  'GetShaderiv': {'type': 'GETn'},
  'GetShaderInfoLog': {'type': 'STRn'},
  'GetShaderPrecisionFormat': {'type': 'Custom'},
  'GetShaderSource': {'type': 'STRn'},
  'GetTexParameterfv': {'type': 'GETn'},
  'GetTexParameteriv': {'type': 'GETn'},
  'GetUniformfv': {'type': 'Custom', 'immediate': False},
  'GetUniformiv': {'type': 'Custom', 'immediate': False},
  'GetUniformLocation': {
    'type': 'HandWritten',
    'immediate': True,
    'needs_size': True,
    'cmd_args':
        'GLResourceId program, const char* name, NonImmediate GLint* location',
  },
  'GetVertexAttribfv': {'type': 'GETn'},
  'GetVertexAttribiv': {'type': 'GETn'},
  'GetVertexAttribPointerv': {'type': 'Custom', 'immediate': False},
  'IsBuffer': {'type': 'Is'},
  'IsEnabled': {'type': 'Is'},
  'IsFramebuffer': {'type': 'Is', 'DecoderFunc': 'glIsFramebufferEXT'},
  'IsProgram': {'type': 'Is'},
  'IsRenderbuffer': {'type': 'Is', 'DecoderFunc': 'glIsRenderbufferEXT'},
  'IsShader': {'type': 'Is'},
  'IsTexture': {'type': 'Is'},
  'PixelStorei': {'type': 'Manual'},
  'RenderbufferStorage': {'DecoderFunc': 'glRenderbufferStorageEXT'},
  'ReadPixels': {'type': 'Custom', 'immediate': False},
  'ReleaseShaderCompiler': {'type': 'Noop'},
  'ShaderBinary': {'type': 'Noop'},
  'ShaderSource': {
    'type': 'Manual',
    'immediate': True,
    'needs_size': True,
    'cmd_args':
        'GLuint shader, GLsizei count, const char* data',
  },
  'TexImage2D': {'type': 'Manual', 'immediate': True},
  'TexParameterfv': {'type': 'PUT', 'data_type': 'GLfloat', 'count': 1},
  'TexParameteriv': {'type': 'PUT', 'data_type': 'GLint', 'count': 1},
  'TexSubImage2D': {'type': 'Data'},
  'Uniform1fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 1},
  'Uniform1iv': {'type': 'PUTn', 'data_type': 'GLint', 'count': 1},
  'Uniform2fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 2},
  'Uniform2iv': {'type': 'PUTn', 'data_type': 'GLint', 'count': 2},
  'Uniform3fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 3},
  'Uniform3iv': {'type': 'PUTn', 'data_type': 'GLint', 'count': 3},
  'Uniform4fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 4},
  'Uniform4iv': {'type': 'PUTn', 'data_type': 'GLint', 'count': 4},
  'UniformMatrix2fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 4},
  'UniformMatrix3fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 9},
  'UniformMatrix4fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 16},
  'VertexAttrib1fv': {'type': 'PUT', 'data_type': 'GLfloat', 'count': 1},
  'VertexAttrib2fv': {'type': 'PUT', 'data_type': 'GLfloat', 'count': 2},
  'VertexAttrib3fv': {'type': 'PUT', 'data_type': 'GLfloat', 'count': 3},
  'VertexAttrib4fv': {'type': 'PUT', 'data_type': 'GLfloat', 'count': 4},
  'VertexAttribPointer': {
      'type': 'Manual',
      'cmd_args': 'GLuint indx, GLint size, GLenum type, GLboolean normalized, '
                  'GLsizei stride, GLuint offset',
  },
  'SwapBuffers': {'DecoderFunc': 'DoSwapBuffers'},
}


class CWriter(object):
  """Writes to a file formatting it for Google's style guidelines."""

  def __init__(self, filename):
    self.filename = filename
    self.file = open(filename, "w")

  def Write(self, string):
    """Writes a string to a file spliting if it's > 80 characters."""
    lines = string.splitlines()
    num_lines = len(lines)
    for ii in range(0, num_lines):
      self.__WriteLine(lines[ii], ii < (num_lines - 1) or string[-1] == '\n')

  def __FindSplit(self, string):
    """Finds a place to split a string."""
    splitter = string.find('=')
    if splitter >= 0 and not string[splitter + 1] == '=':
      return splitter
    parts = string.split('(')
    if len(parts) > 1:
      splitter = len(parts[0])
      for ii in range(1, len(parts)):
        if not parts[ii - 1][-3:] == "if ":
          return splitter
        splitter += len(parts[ii]) + 1
    done = False
    end = len(string)
    last_splitter = -1
    while not done:
      splitter = string[0:end].rfind(',')
      if splitter < 0:
        return last_splitter
      elif splitter >= 80:
        end = splitter
      else:
        return splitter

  def __WriteLine(self, line, ends_with_eol):
    """Given a signle line, writes it to a file, splitting if it's > 80 chars"""
    if len(line) >= 80:
      i = self.__FindSplit(line)
      if i > 0:
        line1 = line[0:i + 1]
        self.file.write(line1 + '\n')
        match = re.match("( +)", line1)
        indent = ""
        if match:
          indent = match.group(1)
        splitter = line[i]
        if not splitter == ',':
          indent = "    " + indent
        self.__WriteLine(indent + line[i + 1:].lstrip(), True)
        return
    self.file.write(line)
    if ends_with_eol:
      self.file.write('\n')

  def Close(self):
    """Close the file."""
    self.file.close()


class TypeHandler(object):
  """This class emits code for a particular type of function."""

  def __init__(self):
    pass

  def InitFunction(self, func):
    """Add or adjust anything type specific for this function."""
    if func.GetInfo('needs_size'):
      func.AddCmdArg(Argument('data_size', 'uint32'))

  def AddImmediateFunction(self, generator, func):
    """Adds an immediate version of a function."""
    # Generate an immediate command if there is only 1 pointer arg.
    immediate = func.GetInfo('immediate')  # can be True, False or None
    if immediate == True or immediate == None:
      if func.num_pointer_args == 1 or immediate:
        generator.AddFunction(ImmediateFunction(func))

  def WriteStruct(self, func, file):
    """Writes a structure that matches the arguments to a function."""
    file.Write("struct %s {\n" % func.name)
    file.Write("  typedef %s ValueType;\n" % func.name)
    file.Write("  static const CommandId kCmdId = k%s;\n" % func.name)
    func.WriteCmdArgFlag(file)
    file.Write("\n")

    func.WriteCmdComputeSize(file)
    func.WriteCmdSetHeader(file)
    func.WriteCmdInit(file)
    func.WriteCmdSet(file)

    file.Write("  command_buffer::CommandHeader header;\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("  %s %s;\n" % (arg.cmd_type, arg.name))
    file.Write("};\n")
    file.Write("\n")

    size = len(args) * _SIZE_OF_UINT32 + _SIZE_OF_COMMAND_HEADER
    file.Write("COMPILE_ASSERT(sizeof(%s) == %d,\n" % (func.name, size))
    file.Write("               Sizeof_%s_is_not_%d);\n" % (func.name, size))
    file.Write("COMPILE_ASSERT(offsetof(%s, header) == 0,\n" % func.name)
    file.Write("               OffsetOf_%s_header_not_0);\n" % func.name)
    offset = _SIZE_OF_COMMAND_HEADER
    for arg in args:
      file.Write("COMPILE_ASSERT(offsetof(%s, %s) == %d,\n" %
                 (func.name, arg.name, offset))
      file.Write("               OffsetOf_%s_%s_not_%d);\n" %
                 (func.name, arg.name, offset))
      offset += _SIZE_OF_UINT32
    file.Write("\n")

  def WriteHandlerImplementation(self, func, file):
    """Writes the handler implementation for this command."""
    file.Write("  parse_error::ParseError result =\n")
    file.Write("      Validate%s(this, immediate_data_size%s);\n" %
               (func.name, func.MakeOriginalArgString("", True)))
    file.Write("  if (result != parse_error::kParseNoError) {\n")
    file.Write("    return result;\n")
    file.Write("  }\n")
    file.Write("  %s(%s);\n" %
               (func.GetGLFunctionName(), func.MakeOriginalArgString("")))

  def WriteCmdSizeTest(self, func, file):
    """Writes the size test for a command."""
    file.Write("  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4);  // NOLINT\n")

  def WriteFormatTest(self, func, file):
    """Writes a format test for a command."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  %s cmd = { 0, };\n" % func.name)
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 11
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(");\n")
    value = 11
    file.Write("  EXPECT_EQ(%s::kCmdId, cmd.header.command);\n" % func.name)
    func.type_handler.WriteCmdSizeTest(func, file)
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateFormatTest(self, func, file):
    """Writes a format test for an immediate version of a command."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  int8 buf[256] = { 0, };\n")
    file.Write("  %s& cmd = *static_cast<%s*>(static_cast<void*>(&buf));\n" %
               (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 11
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(");\n")
    value = 11
    file.Write("  EXPECT_EQ(%s::kCmdId, cmd.header.command);\n" % func.name)
    func.type_handler.WriteImmediateCmdSizeTest(func, file)
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateCmdSizeTest(self, func, file):
    """Writes a size test for an immediate version of a command."""
    file.Write("  // TODO(gman): Compute correct size.\n")
    file.Write("  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4);\n")

  def WriteImmediateHandlerImplementation (self, func, file):
    """Writes the handler impl for the immediate version of a command."""
    file.Write("  // Immediate version.\n")
    file.Write("  parse_error::ParseError result =\n")
    file.Write("      Validate%s(this, immediate_data_size%s);\n" %
               (func.name, func.MakeOriginalArgString("", True)))
    file.Write("  if (result != parse_error::kParseNoError) {\n")
    file.Write("    return result;\n")
    file.Write("  }\n")
    file.Write("  %s(%s);\n" %
               (func.GetGLFunctionName(), func.MakeOriginalArgString("")))

  def WriteServiceImplementation(self, func, file):
    """Writes the service implementation for a command."""
    file.Write(
        "parse_error::ParseError GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    for arg in func.GetOriginalArgs():
      arg.WriteGetCode(file)
    func.WriteHandlerImplementation(file)
    file.Write("  return parse_error::kParseNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateServiceImplementation(self, func, file):
    """Writes the service implementation for an immediate version of command."""
    file.Write(
        "parse_error::ParseError GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    for arg in func.GetOriginalArgs():
      arg.WriteGetCode(file)
    func.WriteHandlerImplementation(file)
    file.Write("  return parse_error::kParseNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateValidationCode(self, func, file):
    """Writes the validation code for an immediate version of a command."""
    pass

  def WriteGLES2ImplementationHeader(self, func, file):
    """Writes the GLES2 Implemention declaration."""
    if func.can_auto_generate:
      file.Write("%s %s(%s) {\n" %
                 (func.return_type, func.original_name,
                  func.MakeTypedOriginalArgString("")))
      file.Write("  helper_->%s(%s);\n" %
                 (func.name, func.MakeOriginalArgString("")))
      file.Write("}\n")
      file.Write("\n")
    else:
      file.Write("%s %s(%s);\n" %
                 (func.return_type, func.original_name,
                  func.MakeTypedOriginalArgString("")))
      file.Write("\n")

  def WriteGLES2ImplementationImpl(self, func, file):
    """Writes the GLES2 Implemention definition."""
    if not func.can_auto_generate:
      file.Write("%s GLES2Implementation::%s(%s) {\n" %
                 (func.return_type, func.original_name,
                  func.MakeTypedOriginalArgString("")))
      if not func.return_type == "void":
        file.Write("  return 0;\n")
      file.Write("}\n")
      file.Write("\n")

  def WriteImmediateCmdComputeSize(self, func, file):
    """Writes the size computation code for the immediate version of a cmd."""
    file.Write("  static uint32 ComputeSize(uint32 size_in_bytes) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) +  // NOLINT\n")
    file.Write("        RoundSizeToMultipleOfEntries(size_in_bytes));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Writes the SetHeader function for the immediate version of a cmd."""
    file.Write("  void SetHeader(uint32 size_in_bytes) {\n")
    file.Write("    header.SetCmdByTotalSize<ValueType>(size_in_bytes);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Writes the Init function for the immediate version of a command."""
    file.Write("  void Init(%s) {\n" % func.MakeTypedCmdArgString("_"))
    file.Write("    SetHeader(0);  // TODO(gman): pass in correct size\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Writes the Set function for the immediate version of a command."""
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s) {\n" %
               func.MakeTypedCmdArgString("_", True))
    file.Write("    // TODO(gman): compute correct size.\n")
    file.Write("    const uint32 size = ComputeSize(0);\n")
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s);\n" % copy_args)
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteCmdHelper(self, func, file):
    """Writes the cmd helper definition for a cmd."""
    args = func.MakeCmdArgString("")
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedCmdArgString("")))
    file.Write("    gles2::%s& c = GetCmdSpace<gles2::%s>();\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % args)
    file.Write("  }\n\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Writes the cmd helper definition for the immediate version of a cmd."""
    args = func.MakeCmdArgString("")
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedCmdArgString("")))
    file.Write("    const uint32 s = 0;  // TODO(gman): compute correct size\n")
    file.Write("    gles2::%s& c = "
               "GetImmediateCmdSpaceTotalSize<gles2::%s>(s);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % args)
    file.Write("  }\n\n")


class CustomHandler(TypeHandler):
  """Handler for commands that are auto-generated but require minor tweaks."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateCmdGetTotalSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("    uint32 total_size = 0;  // TODO(gman): get correct size.\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void Init(%s) {\n" % func.MakeTypedCmdArgString("_"))
    self.WriteImmediateCmdGetTotalSize(func, file)
    file.Write("    SetHeader(total_size);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s) {\n" %
               func.MakeTypedCmdArgString("_", True))
    self.WriteImmediateCmdGetTotalSize(func, file)
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s);\n" % copy_args)
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, total_size);\n")
    file.Write("  }\n")
    file.Write("\n")


class TodoHandler(CustomHandler):
  """Handle for commands that are not yet implemented."""

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    pass


class HandWrittenHandler(CustomHandler):
  """Handler for comands where everything must be written by hand."""

  def InitFunction(self, func):
    """Add or adjust anything type specific for this function."""
    CustomHandler.InitFunction(self, func)
    func.can_auto_generate = False

  def WriteStruct(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteGLES2ImplementationImpl(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): Write test for %s\n" % func.name)

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): Write test for %s\n" % func.name)


class ManualHandler(CustomHandler):
  """Handler for commands who's handlers must be written by hand."""

  def __init__(self):
    CustomHandler.__init__(self)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): Implement test for %s\n" % func.name)

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s);\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("\n")

  def WriteGLES2ImplementationImpl(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateCmdGetTotalSize(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Move this data to _FUNCTION_INFO?
    if func.name == 'ShaderSourceImmediate':
      file.Write("    uint32 total_size = ComputeSize(_data_size);\n")
    else:
      CustomHandler.WriteImmediateCmdGetTotalSize(self, func, file)


class DataHandler(CustomHandler):
  """Handler for glBufferData, glBufferSubData, glTexImage2D, glTexSubImage2D,
     glCompressedTexImage2D, glCompressedTexImageSub2D."""
  def __init__(self):
    CustomHandler.__init__(self)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "parse_error::ParseError GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    for arg in func.GetCmdArgs():
      arg.WriteGetCode(file)

    # TODO(gman): Move this data to _FUNCTION_INFO?
    if func.name == 'BufferData':
      file.Write("  uint32 data_size = size;\n")
    elif func.name == 'BufferSubData':
      file.Write("  uint32 data_size = size;\n")
    elif func.name == 'CompressedTexImage2D':
      file.Write("  uint32 data_size = imageSize;\n")
    elif func.name == 'CompressedTexSubImage2D':
      file.Write("  uint32 data_size = imageSize;\n")
    elif func.name == 'TexImage2D':
      file.Write("  uint32 pixels_size = GLES2Util::ComputeImageDataSize(\n")
      file.Write("      width, height, format, type, unpack_alignment_);\n")
    elif func.name == 'TexSubImage2D':
      file.Write("  uint32 pixels_size = GLES2Util::ComputeImageDataSize(\n")
      file.Write("      width, height, format, type, unpack_alignment_);\n")
    else:
      file.Write("  uint32 data_size = 0;  // TODO(gman): get correct size!\n")

    for arg in func.GetOriginalArgs():
      arg.WriteGetAddress(file)
    func.WriteHandlerImplementation(file)
    file.Write("  return parse_error::kParseNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateCmdGetTotalSize(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Move this data to _FUNCTION_INFO?
    if func.name == 'BufferDataImmediate':
      file.Write("    uint32 total_size = ComputeSize(_size);\n")
    elif func.name == 'BufferSubDataImmediate':
        file.Write("    uint32 total_size = ComputeSize(_size);\n")
    elif func.name == 'CompressedTexImage2DImmediate':
        file.Write("    uint32 total_size = ComputeSize(_imageSize);\n")
    elif func.name == 'CompressedTexSubImage2DImmediate':
        file.Write("    uint32 total_size = ComputeSize(_imageSize);\n")
    elif func.name == 'TexImage2DImmediate':
      file.Write(
          "    uint32 total_size = 0;  // TODO(gman): get correct size\n")
    elif func.name == 'TexSubImage2DImmediate':
      file.Write(
          "    uint32 total_size = 0;  // TODO(gman): get correct size\n")

  def WriteImmediateCmdSizeTest(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Move this data to _FUNCTION_INFO?
    if func.name == 'BufferDataImmediate':
      file.Write("    uint32 total_size = cmd.ComputeSize(cmd.size);\n")
    elif func.name == 'BufferSubDataImmediate':
        file.Write("    uint32 total_size = cmd.ComputeSize(cmd.size);\n")
    elif func.name == 'CompressedTexImage2DImmediate':
        file.Write("    uint32 total_size = cmd.ComputeSize(cmd.imageSize);\n")
    elif func.name == 'CompressedTexSubImage2DImmediate':
        file.Write("    uint32 total_size = cmd.ComputeSize(cmd.imageSize);\n")
    elif func.name == 'TexImage2DImmediate':
      file.Write(
          "    uint32 total_size = 0;  // TODO(gman): get correct size\n")
    elif func.name == 'TexSubImage2DImmediate':
      file.Write(
          "    uint32 total_size = 0;  // TODO(gman): get correct size\n")
    file.Write("  EXPECT_EQ(sizeof(cmd), total_size);\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Remove this exception.
    file.Write("// TODO(gman): Implement test for %s\n" % func.name)
    return

  def WriteGLES2ImplementationImpl(self, func, file):
    """Overrriden from TypeHandler."""
    pass


class GENnHandler(TypeHandler):
  """Handler for glGen___ type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    pass

  def WriteHandlerImplementation (self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  parse_error::ParseError result =\n")
    file.Write("      Validate%s(this, immediate_data_size%s);\n" %
               (func.name, func.MakeOriginalArgString("", True)))
    file.Write("  if (result != parse_error::kParseNoError) {\n")
    file.Write("    return result;\n")
    file.Write("  }\n")
    file.Write("  GenGLObjects<GL%sHelper>(n, %s);\n" %
               (func.name, func.GetLastOriginalArg().name))

  def WriteImmediateHandlerImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  parse_error::ParseError result =\n")
    file.Write("      Validate%s(this, immediate_data_size%s);\n" %
               (func.name, func.MakeOriginalArgString("", True)))
    file.Write("  if (result != parse_error::kParseNoError) {\n")
    file.Write("    return result;\n")
    file.Write("  }\n")
    file.Write("  GenGLObjects<GL%sHelper>(n, %s);\n" %
               (func.original_name, func.GetLastOriginalArg().name))

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  MakeIds(%s);\n" % func.MakeOriginalArgString(""))
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationImpl(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize(GLsizei n) {\n")
    file.Write(
        "    return static_cast<uint32>(sizeof(GLuint) * n);  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize(GLsizei n) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) + ComputeDataSize(n));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader(GLsizei n) {\n")
    file.Write("    header.SetCmdByTotalSize<ValueType>(ComputeSize(n));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    file.Write("    SetHeader(_n);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    memcpy(ImmediateDataAddress(this),\n")
    file.Write("           _%s, ComputeDataSize(_n));\n" % last_arg.name)
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    file.Write("    const uint32 size = ComputeSize(_n);\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    args = func.MakeOriginalArgString("")
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedOriginalArgString("")))
    file.Write("    const uint32 size = gles2::%s::ComputeSize(n);\n" %
               func.name)
    file.Write("    gles2::%s& c = "
               "GetImmediateCmdSpaceTotalSize<gles2::%s>(size);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % args)
    file.Write("  }\n\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  static GLuint ids[] = { 12, 23, 34, };\n")
    file.Write("  int8 buf[256] = { 0, };\n")
    file.Write("  %s& cmd = *static_cast<%s*>(static_cast<void*>(&buf));\n" %
               (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 11
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(",\n      ids);\n")
    args = func.GetCmdArgs()
    value = 11
    file.Write("  EXPECT_EQ(%s::kCmdId, cmd.header.command);\n" % func.name)
    file.Write("  EXPECT_EQ(sizeof(cmd) +\n")
    file.Write("            RoundSizeToMultipleOfEntries(cmd.n * 4),\n")
    file.Write("            cmd.header.size * 4);  // NOLINT\n")
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  // TODO(gman): Check that ids were inserted;\n")
    file.Write("}\n")
    file.Write("\n")


class CreateHandler(TypeHandler):
  """Handler for glCreate___ type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    func.AddCmdArg(Argument("client_id", 'uint32'))

  def WriteHandlerImplementation (self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  uint32 client_id = c.client_id;\n")
    file.Write("  parse_error::ParseError result =\n")
    file.Write("      Validate%s(this, immediate_data_size%s);\n" %
               (func.name, func.MakeOriginalArgString("", True)))
    file.Write("  if (result != parse_error::kParseNoError) {\n")
    file.Write("    return result;\n")
    file.Write("  }\n")
    file.Write("  %sHelper(%s);\n" %
               (func.name, func.MakeCmdArgString("")))

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  GLuint client_id;\n")
    file.Write("  MakeIds(1, &client_id);\n")
    file.Write("  helper_->%s(%s);\n" %
               (func.name, func.MakeCmdArgString("")))
    file.Write("  return client_id;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationImpl(self, func, file):
    """Overrriden from TypeHandler."""
    pass


class DELnHandler(TypeHandler):
  """Handler for glDelete___ type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteHandlerImplementation (self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  parse_error::ParseError result =\n")
    file.Write("      Validate%s(this, immediate_data_size%s);\n" %
               (func.name, func.MakeOriginalArgString("", True)))
    file.Write("  if (result != parse_error::kParseNoError) {\n")
    file.Write("    return result;\n")
    file.Write("  }\n")
    file.Write("  DeleteGLObjects<GL%sHelper>(n, %s);\n" %
               (func.name, func.GetLastOriginalArg().name))

  def WriteImmediateHandlerImplementation (self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  parse_error::ParseError result =\n")
    file.Write("      Validate%s(this, immediate_data_size%s);\n" %
               (func.name, func.MakeOriginalArgString("", True)))
    file.Write("  if (result != parse_error::kParseNoError) {\n")
    file.Write("    return result;\n")
    file.Write("  }\n")
    file.Write("  DeleteGLObjects<GL%sHelper>(n, %s);\n" %
               (func.original_name, func.GetLastOriginalArg().name))

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  FreeIds(%s);\n" % func.MakeOriginalArgString(""))
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationImpl(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize(GLsizei n) {\n")
    file.Write(
        "    return static_cast<uint32>(sizeof(GLuint) * n);  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize(GLsizei n) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) + ComputeDataSize(n));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader(GLsizei n) {\n")
    file.Write("    header.SetCmdByTotalSize<ValueType>(ComputeSize(n));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    file.Write("    SetHeader(_n);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    memcpy(ImmediateDataAddress(this),\n")
    file.Write("           _%s, ComputeDataSize(_n));\n" % last_arg.name)
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    file.Write("    const uint32 size = ComputeSize(_n);\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    args = func.MakeOriginalArgString("")
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedOriginalArgString("")))
    file.Write("    const uint32 size = gles2::%s::ComputeSize(n);\n" %
               func.name)
    file.Write("    gles2::%s& c = "
               "GetImmediateCmdSpaceTotalSize<gles2::%s>(size);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % args)
    file.Write("  }\n\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  static GLuint ids[] = { 12, 23, 34, };\n")
    file.Write("  int8 buf[256] = { 0, };\n")
    file.Write("  %s& cmd = *static_cast<%s*>(static_cast<void*>(&buf));\n" %
               (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 11
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(",\n      ids);\n")
    args = func.GetCmdArgs()
    value = 11
    file.Write("  EXPECT_EQ(%s::kCmdId, cmd.header.command);\n" % func.name)
    file.Write("  EXPECT_EQ(sizeof(cmd) +\n")
    file.Write("            RoundSizeToMultipleOfEntries(cmd.n * 4),\n")
    file.Write("            cmd.header.size * 4);  // NOLINT\n")
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  // TODO(gman): Check that ids were inserted;\n")
    file.Write("}\n")
    file.Write("\n")


class GETnHandler(TypeHandler):
  """Handler for GETn for glGetBooleanv, glGetFloatv, ... type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def AddImmediateFunction(self, generator, func):
    """Overrriden from TypeHandler."""
    pass

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "parse_error::ParseError GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()

    all_but_last_args = func.GetOriginalArgs()[:-1]
    for arg in all_but_last_args:
      arg.WriteGetCode(file)

    file.Write("  %s params;\n" % last_arg.type)
    file.Write("  GLsizei num_values = util_.GLGetNumValuesReturned(pname);\n")
    file.Write("  uint32 params_size = num_values * sizeof(*params);\n")
    file.Write("  params = GetSharedMemoryAs<%s>(\n" % last_arg.type)
    file.Write("      c.params_shm_id, c.params_shm_offset, params_size);\n")
    func.WriteHandlerImplementation(file)
    file.Write("  return parse_error::kParseNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    all_but_last_args = func.GetOriginalArgs()[:-1]
    arg_string = (
        ", ".join(["%s" % arg.name for arg in all_but_last_args]))
    file.Write("  helper_->%s(%s, shared_memory_.GetId(), 0);\n" %
               (func.name, arg_string))
    file.Write("  int32 token = helper_->InsertToken();\n")
    file.Write("  helper_->WaitForToken(token);\n")
    file.Write("  GLsizei num_values = util_.GLGetNumValuesReturned(pname);\n")
    file.Write("  memcpy(params, shared_memory_.GetAddress(0),\n")
    file.Write("         num_values * sizeof(*params));\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationImpl(self, func, file):
    """Overrriden from TypeHandler."""
    pass


class PUTHandler(TypeHandler):
  """Handler for glTexParameter_v, glVertexAttrib_v functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteImmediateValidationCode(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  if (!CheckImmediateDataSize<%s>("
               "immediate_data_size, 1, sizeof(%s), %d)) {\n" %
               (func.name, func.info.data_type, func.info.count))
    file.Write("    return parse_error::kParseOutOfBounds;\n")
    file.Write("  }\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationImpl(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize() {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(%s) * %d);  // NOLINT\n" %
               (func.info.data_type, func.info.count))
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize() {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write(
        "        sizeof(ValueType) + ComputeDataSize());  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader() {\n")
    file.Write(
        "    header.SetCmdByTotalSize<ValueType>(ComputeSize());\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    file.Write("    SetHeader();\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    memcpy(ImmediateDataAddress(this),\n")
    file.Write("           _%s, ComputeDataSize());\n" % last_arg.name)
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    file.Write("    const uint32 size = ComputeSize();\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    args = func.MakeOriginalArgString("")
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedOriginalArgString("")))
    file.Write("    const uint32 size = gles2::%s::ComputeSize();\n" %
               func.name)
    file.Write("    gles2::%s& c = "
               "GetImmediateCmdSpaceTotalSize<gles2::%s>(size);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % args)
    file.Write("  }\n\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  const int kSomeBaseValueToTestWith = 51;\n")
    file.Write("  static %s data[] = {\n" % func.info.data_type)
    for v in range(0, func.info.count):
      file.Write("    static_cast<%s>(kSomeBaseValueToTestWith + %d),\n" %
                 (func.info.data_type, v))
    file.Write("  };\n")
    file.Write("  int8 buf[256] = { 0, };\n")
    file.Write("  %s& cmd = *static_cast<%s*>(static_cast<void*>(&buf));\n" %
               (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 11
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(",\n      data);\n")
    args = func.GetCmdArgs()
    value = 11
    file.Write("  EXPECT_EQ(%s::kCmdId, cmd.header.command);\n" % func.name)
    file.Write("  EXPECT_EQ(sizeof(cmd) +\n")
    file.Write("            RoundSizeToMultipleOfEntries(sizeof(data)),\n")
    file.Write("            cmd.header.size * 4);  // NOLINT\n")
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  // TODO(gman): Check that data was inserted;\n")
    file.Write("}\n")
    file.Write("\n")


class PUTnHandler(TypeHandler):
  """Handler for PUTn 'glUniform__v' type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteImmediateValidationCode(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  if (!CheckImmediateDataSize<%s>("
               "immediate_data_size, count, sizeof(%s), %d)) {\n" %
               (func.name, func.info.data_type, func.info.count))
    file.Write("    return parse_error::kParseOutOfBounds;\n")
    file.Write("  }\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationImpl(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize(GLsizei count) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(%s) * %d * count);  // NOLINT\n" %
               (func.info.data_type, func.info.count))
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize(GLsizei count) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write(
        "        sizeof(ValueType) + ComputeDataSize(count));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader(GLsizei count) {\n")
    file.Write(
        "    header.SetCmdByTotalSize<ValueType>(ComputeSize(count));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    file.Write("    SetHeader(_count);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    memcpy(ImmediateDataAddress(this),\n")
    file.Write("           _%s, ComputeDataSize(_count));\n" % last_arg.name)
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    file.Write("    const uint32 size = ComputeSize(_count);\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    args = func.MakeOriginalArgString("")
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedOriginalArgString("")))
    file.Write("    const uint32 size = gles2::%s::ComputeSize(count);\n" %
               func.name)
    file.Write("    gles2::%s& c = "
               "GetImmediateCmdSpaceTotalSize<gles2::%s>(size);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % args)
    file.Write("  }\n\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  const int kSomeBaseValueToTestWith = 51;\n")
    file.Write("  static %s data[] = {\n" % func.info.data_type)
    for v in range(0, func.info.count * 2):
      file.Write("    static_cast<%s>(kSomeBaseValueToTestWith + %d),\n" %
                 (func.info.data_type, v))
    file.Write("  };\n")
    file.Write("  int8 buf[256] = { 0, };\n")
    file.Write("  %s& cmd = *static_cast<%s*>(static_cast<void*>(&buf));\n" %
               (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 1
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(",\n      data);\n")
    args = func.GetCmdArgs()
    value = 1
    file.Write("  EXPECT_EQ(%s::kCmdId, cmd.header.command);\n" % func.name)
    file.Write("  EXPECT_EQ(sizeof(cmd) +\n")
    file.Write("            RoundSizeToMultipleOfEntries(sizeof(data)),\n")
    file.Write("            cmd.header.size * 4);  // NOLINT\n")
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  // TODO(gman): Check that data was inserted;\n")
    file.Write("}\n")
    file.Write("\n")


class GLcharHandler(TypeHandler):
  """Handler for functions that pass a single string ."""

  def __init__(self):
    TypeHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    func.AddCmdArg(Argument('data_size', 'uint32'))

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "parse_error::ParseError GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()

    all_but_last_arg = func.GetOriginalArgs()[:-1]
    for arg in all_but_last_arg:
      arg.WriteGetCode(file)

    file.Write("  uint32 name_size = c.data_size;\n")
    file.Write("  const char* name = GetSharedMemoryAs<%s>(\n" %
               last_arg.type)
    file.Write("      c.%s_shm_id, c.%s_shm_offset, name_size);\n" %
               (last_arg.name, last_arg.name))
    file.Write("  parse_error::ParseError result =\n")
    file.Write("      Validate%s(this, immediate_data_size%s);\n" %
               (func.name, func.MakeOriginalArgString("", True)))
    file.Write("  if (result != parse_error::kParseNoError) {\n")
    file.Write("    return result;\n")
    file.Write("  }\n")
    arg_string = ", ".join(["%s" % arg.name for arg in all_but_last_arg])
    file.Write("  String name_str(name, name_size);\n")
    file.Write("  %s(%s, name_str.c_str());\n" %
               (func.GetGLFunctionName(), arg_string))
    file.Write("  return parse_error::kParseNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "parse_error::ParseError GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()

    all_but_last_arg = func.GetOriginalArgs()[:-1]
    for arg in all_but_last_arg:
      arg.WriteGetCode(file)

    file.Write("  uint32 name_size = c.data_size;\n")
    file.Write(
        "  const char* name = GetImmediateDataAs<const char*>(c);\n")
    file.Write("  // TODO(gman): Make sure validate checks\n")
    file.Write("  //     immediate_data_size covers data_size.\n")
    file.Write("  parse_error::ParseError result =\n")
    file.Write("      Validate%s(this, immediate_data_size%s);\n" %
               (func.name, func.MakeOriginalArgString("", True)))
    file.Write("  if (result != parse_error::kParseNoError) {\n")
    file.Write("    return result;\n")
    file.Write("  }\n")
    arg_string = ", ".join(["%s" % arg.name for arg in all_but_last_arg])
    file.Write("  String name_str(name, name_size);\n")
    file.Write("  %s(%s, name_str.c_str());\n" %
              (func.GetGLFunctionName(), arg_string))
    file.Write("  return parse_error::kParseNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  // TODO(gman): This needs to change to use SendString.\n")
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationImpl(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize(const char* s) {\n")
    file.Write("    return strlen(s);\n")
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize(const char* s) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) + ComputeDataSize(s));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader(const char* s) {\n")
    file.Write("    header.SetCmdByTotalSize<ValueType>(ComputeSize(s));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void Init(%s) {\n" % func.MakeTypedOriginalArgString("_"))
    file.Write("    SetHeader(_%s);\n" % last_arg.name)
    args = func.GetCmdArgs()[:-1]
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    data_size = strlen(_%s);\n" % last_arg.name)
    file.Write("    memcpy(ImmediateDataAddress(this), _%s, data_size);\n" %
               last_arg.name)
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void* Set(void* cmd%s) {\n" %
               func.MakeTypedOriginalArgString("_", True))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s);\n" %
               func.MakeOriginalArgString("_"))
    file.Write("    const uint32 size = ComputeSize(_%s);\n" % last_arg.name)
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    args = func.MakeOriginalArgString("")
    last_arg = func.GetLastOriginalArg()
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedOriginalArgString("")))
    file.Write("    const uint32 size = gles2::%s::ComputeSize(%s);\n" %
               (func.name, last_arg.name))
    file.Write("    gles2::%s& c = GetImmediateCmdSpaceTotalSize<gles2::%s>("
               "size);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % args)
    file.Write("  }\n\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  int8 buf[256] = { 0, };\n")
    file.Write("  %s& cmd = *static_cast<%s*>(static_cast<void*>(&buf));\n" %
               (func.name, func.name))
    file.Write("  static const char* const test_str = \"test string\";\n")
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    all_but_last_arg = func.GetCmdArgs()[:-1]
    value = 11
    for arg in all_but_last_arg:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(",\n      test_str);\n")
    value = 11
    file.Write("  EXPECT_EQ(%s::kCmdId, cmd.header.command);\n" % func.name)
    file.Write("  EXPECT_EQ(sizeof(cmd) +  // NOLINT\n")
    file.Write("            RoundSizeToMultipleOfEntries(strlen(test_str)),\n")
    file.Write("            cmd.header.size * 4);\n")
    for arg in all_but_last_arg:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  // TODO(gman): check that string got copied.\n")
    file.Write("}\n")
    file.Write("\n")

class GetGLcharHandler(GLcharHandler):
  """Handler for glGetAttibLoc, glGetUniformLoc."""

  def __init__(self):
    GLcharHandler.__init__(self)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "parse_error::ParseError GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()

    all_but_last_arg = func.GetOriginalArgs()
    for arg in all_but_last_arg:
      arg.WriteGetCode(file)

    file.Write("  uint32 name_size = c.data_size;\n")
    file.Write("  const char* name = GetSharedMemoryAs<%s>(\n" %
               last_arg.type)
    file.Write("      c.%s_shm_id, c.%s_shm_offset, name_size);\n" %
               (last_arg.name, last_arg.name))
    file.Write("  GLint* location = GetSharedMemoryAs<GLint*>(\n")
    file.Write(
        "      c.location_shm_id, c.location_shm_offset, sizeof(*location));\n")
    file.Write("  // TODO(gman): Validate location.\n")
    file.Write("  parse_error::ParseError result =\n")
    file.Write("      Validate%s(this, immediate_data_size%s);\n" %
               (func.name, func.MakeOriginalArgString("", True)))
    file.Write("  if (result != parse_error::kParseNoError) {\n")
    file.Write("    return result;\n")
    file.Write("  }\n")
    arg_string = ", ".join(["%s" % arg.name for arg in all_but_last_arg])
    file.Write("  String name_str(name, name_size);\n")
    file.Write("  *location = %s(%s, name_str.c_str());\n" %
               (func.GetGLFunctionName(), arg_string))
    file.Write("  return parse_error::kParseNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "parse_error::ParseError GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()

    all_but_last_arg = func.GetOriginalArgs()[:-1]
    for arg in all_but_last_arg:
      arg.WriteGetCode(file)

    file.Write("  uint32 name_size = c.data_size;\n")
    file.Write(
        "  const char* name = GetImmediateDataAs<const char*>(c);\n")
    file.Write("  // TODO(gman): Make sure validate checks\n")
    file.Write("  //     immediate_data_size covers data_size.\n")
    file.Write("  GLint* location = GetSharedMemoryAs<GLint*>(\n")
    file.Write(
        "      c.location_shm_id, c.location_shm_offset, sizeof(*location));\n")
    file.Write("  // TODO(gman): Validate location.\n")
    file.Write("  parse_error::ParseError result =\n")
    file.Write("      Validate%s(this, immediate_data_size%s);\n" %
               (func.name, func.MakeOriginalArgString("", True)))
    file.Write("  if (result != parse_error::kParseNoError) {\n")
    file.Write("    return result;\n")
    file.Write("  }\n")
    arg_string = ", ".join(["%s" % arg.name for arg in all_but_last_arg])
    file.Write("  String name_str(name, name_size);\n")
    file.Write("  *location = %s(%s, name_str.c_str());\n" %
              (func.GetGLFunctionName(), arg_string))
    file.Write("  return parse_error::kParseNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  // TODO(gman): This needs to change to use SendString.\n")
    file.Write("  GLint* result = shared_memory_.GetAddressAs<GLint*>(0);\n")
    file.Write("  DCHECK(false);  // pass in shared memory\n")
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("  int32 token = helper_->InsertToken();\n")
    file.Write("  helper_->WaitForToken(token);\n")
    file.Write("  return *result;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationImpl(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize(const char* s) {\n")
    file.Write("    return strlen(s);\n")
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize(const char* s) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) + ComputeDataSize(s));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader(const char* s) {\n")
    file.Write("    header.SetCmdByTotalSize<ValueType>(ComputeSize(s));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void Init(%s) {\n" % func.MakeTypedInitString("_"))
    file.Write("    SetHeader(_name);\n")
    args = func.GetInitArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    data_size = ComputeDataSize(_name);\n")
    file.Write("    memcpy(ImmediateDataAddress(this), _name, data_size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void* Set(void* cmd%s) {\n" %
               func.MakeTypedInitString("_", True))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s);\n" %
               func.MakeInitString("_"))
    file.Write("    const uint32 size = ComputeSize(_name);\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedCmdArgString("")))
    file.Write("    const uint32 size = gles2::%s::ComputeSize(name);\n" %
               func.name)
    file.Write("    gles2::%s& c = GetImmediateCmdSpaceTotalSize<gles2::%s>("
               "size);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % func.MakeCmdArgString(""))
    file.Write("  }\n\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  int8 buf[256] = { 0, };\n")
    file.Write("  %s& cmd = *static_cast<%s*>(static_cast<void*>(&buf));\n" %
               (func.name, func.name))
    file.Write("  static const char* const test_str = \"test string\";\n")
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    all_but_last_arg = func.GetCmdArgs()[:-1]
    value = 11
    for arg in all_but_last_arg:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(",\n      test_str);\n")
    value = 11
    file.Write("  EXPECT_EQ(%s::kCmdId, cmd.header.command);\n" % func.name)
    file.Write("  EXPECT_EQ(sizeof(cmd) +  // NOLINT\n")
    file.Write("            RoundSizeToMultipleOfEntries(strlen(test_str)),\n")
    file.Write("            cmd.header.size * 4);\n")
    for arg in all_but_last_arg:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  // TODO(gman): check that string got copied.\n")
    file.Write("}\n")
    file.Write("\n")


class IsHandler(TypeHandler):
  """Handler for glIs____ type and glGetError functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    func.AddCmdArg(Argument("result_shm_id", 'uint32'))
    func.AddCmdArg(Argument("result_shm_offset", 'uint32'))

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "parse_error::ParseError GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    args = func.GetOriginalArgs()
    for arg in args:
      arg.WriteGetCode(file)

    file.Write("  %s* result_dst = GetSharedMemoryAs<%s*>(\n" %
               (func.return_type, func.return_type))
    file.Write(
        "      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));\n")
    file.Write("  parse_error::ParseError result =\n")
    file.Write("      Validate%s(this, immediate_data_size%s);\n" %
               (func.name, func.MakeOriginalArgString("", True)))
    file.Write("  if (result != parse_error::kParseNoError) {\n")
    file.Write("    return result;\n")
    file.Write("  }\n")
    file.Write("  *result_dst = %s(%s);\n" %
               (func.GetGLFunctionName(), func.MakeOriginalArgString("")))
    file.Write("  return parse_error::kParseNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    arg_string = func.MakeOriginalArgString("")
    comma = ""
    if len(arg_string) > 0:
      comma = ", "
    file.Write("  helper_->%s(%s%sshared_memory_.GetId(), 0);\n" %
               (func.name, arg_string, comma))
    file.Write("  int32 token = helper_->InsertToken();\n")
    file.Write("  helper_->WaitForToken(token);\n")
    file.Write("  return *shared_memory_.GetAddressAs<%s*>(0);\n" %
               func.return_type)
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationImpl(self, func, file):
    """Overrriden from TypeHandler."""
    pass


class STRnHandler(TypeHandler):
  """Handler for GetProgramInfoLog, GetShaderInfoLog and GetShaderSource."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): Implement this\n")
    TypeHandler.WriteGLES2ImplementationHeader(self, func, file)


class FunctionInfo(object):
  """Holds info about a function."""

  def __init__(self, info, type_handler):
    for key in info:
      setattr(self, key, info[key])
    self.type_handler = type_handler
    if not 'type' in info:
      self.type = ''


class Argument(object):
  """A class that represents a function argument."""

  cmd_type_map_ = {
    'GLfloat': 'float',
    'GLclampf': 'float',
  }

  def __init__(self, name, type):
    self.name = name
    self.type = type

    if type in self.cmd_type_map_:
      self.cmd_type = self.cmd_type_map_[type]
    else:
      self.cmd_type = 'uint32'

  def IsPointer(self):
    """Returns true if argument is a pointer."""
    return False

  def AddCmdArgs(self, args):
    """Adds command arguments for this argument to the given list."""
    return args.append(self)

  def AddInitArgs(self, args):
    """Adds init arguments for this argument to the given list."""
    return args.append(self)

  def WriteGetCode(self, file):
    """Writes the code to get an argument from a command structure."""
    file.Write("  %s %s = static_cast<%s>(c.%s);\n" %
               (self.type, self.name, self.type, self.name))

  def WriteValidationCode(self, file):
    """Writes the validation code for an argument."""
    pass

  def WriteGetAddress(self, file):
    """Writes the code to get the address this argument refers to."""
    pass

  def GetImmediateVersion(self):
    """Gets the immediate version of this argument."""
    return self


class ImmediatePointerArgument(Argument):
  """A class that represents an immediate argument to a function.

  An immediate argument is one where the data follows the command.
  """

  def __init__(self, name, type):
    Argument.__init__(self, name, type)

  def AddCmdArgs(self, args):
    """Overridden from Argument."""
    pass

  def WriteGetCode(self, file):
    """Overridden from Argument."""
    file.Write(
      "  %s %s = GetImmediateDataAs<%s>(c);\n" %
       (self.type, self.name, self.type))

  def WriteValidationCode(self, file):
    """Overridden from Argument."""
    file.Write("  if (%s == NULL) {\n" % self.name)
    file.Write("    return parse_error::kParseOutOfBounds;\n")
    file.Write("  }\n")

  def GetImmediateVersion(self):
    """Overridden from Argument."""
    return None


class PointerArgument(Argument):
  """A class that represents a pointer argument to a function."""

  def __init__(self, name, type):
    Argument.__init__(self, name, type)

  def IsPointer(self):
    """Returns true if argument is a pointer."""
    return True

  def AddCmdArgs(self, args):
    """Overridden from Argument."""
    args.append(Argument("%s_shm_id" % self.name, 'uint32'))
    args.append(Argument("%s_shm_offset" % self.name, 'uint32'))

  def WriteGetCode(self, file):
    """Overridden from Argument."""
    file.Write(
        "  %s %s = GetSharedMemoryAs<%s>(\n" %
        (self.type, self.name, self.type))
    file.Write(
        "      c.%s_shm_id, c.%s_shm_offset, 0 /* TODO(gman): size */);\n" %
        (self.name, self.name))

  def WriteGetAddress(self, file):
    """Overridden from Argument."""
    file.Write(
        "  %s %s = GetSharedMemoryAs<%s>(\n" %
        (self.type, self.name, self.type))
    file.Write(
        "      %s_shm_id, %s_shm_offset, %s_size);\n" %
        (self.name, self.name, self.name))

  def WriteValidationCode(self, file):
    """Overridden from Argument."""
    file.Write("  if (%s == NULL) {\n" % self.name)
    file.Write("    return parse_error::kParseOutOfBounds;\n")
    file.Write("  }\n")

  def GetImmediateVersion(self):
    """Overridden from Argument."""
    return ImmediatePointerArgument(self.name, self.type)


class NonImmediatePointerArgument(PointerArgument):
  """A pointer argument that stays a pointer even in an immediate cmd."""

  def __init__(self, name, type):
    PointerArgument.__init__(self, name, type)

  def IsPointer(self):
    """Returns true if argument is a pointer."""
    return False

  def GetImmediateVersion(self):
    """Overridden from Argument."""
    return self


class ResourceIdArgument(Argument):
  """A class that represents a resource id argument to a function."""

  def __init__(self, name, type):
    type = type.replace("GLResourceId", "GLuint")
    Argument.__init__(self, name, type)

  def WriteGetCode(self, file):
    """Overridden from Argument."""
    file.Write("  %s %s;\n" % (self.type, self.name))
    file.Write("  if (!id_map_.GetServiceId(c.%s, &%s)) {\n" %
               (self.name, self.name))
    file.Write("    SetGLError(GL_INVALID_VALUE);\n")
    file.Write("    return parse_error::kParseNoError;\n")
    file.Write("  }\n")


class Function(object):
  """A class that represents a function."""

  def __init__(self, name, info, return_type, original_args, args_for_cmds,
               cmd_args, init_args, num_pointer_args):
    self.name = name
    self.original_name = name
    self.info = info
    self.type_handler = info.type_handler
    self.return_type = return_type
    self.original_args = original_args
    self.num_pointer_args = num_pointer_args
    self.can_auto_generate = num_pointer_args == 0 and return_type == "void"
    self.cmd_args = cmd_args
    self.init_args = init_args
    self.args_for_cmds = args_for_cmds
    self.type_handler.InitFunction(self)

  def IsType(self, type_name):
    """Returns true if function is a certain type."""
    return self.info.type == type_name

  def GetInfo(self, name):
    """Returns a value from the function info for this function."""
    if hasattr(self.info, name):
      return getattr(self.info, name)
    return None

  def GetGLFunctionName(self):
    """Gets the function to call to execute GL for this command."""
    if self.GetInfo('DecoderFunc'):
      return self.GetInfo('DecoderFunc')
    return "gl%s" % self.original_name

  def AddCmdArg(self, arg):
    """Adds a cmd argument to this function."""
    self.cmd_args.append(arg)

  def GetCmdArgs(self):
    """Gets the command args for this function."""
    return self.cmd_args

  def GetInitArgs(self):
    """Gets the init args for this function."""
    return self.init_args

  def GetOriginalArgs(self):
    """Gets the original arguments to this function."""
    return self.original_args

  def GetLastOriginalArg(self):
    """Gets the last original argument to this function."""
    return self.original_args[len(self.original_args) - 1]

  def __GetArgList(self, arg_string, add_comma):
    """Adds a comma if arg_string is not empty and add_comma is true."""
    comma = ""
    if add_comma and len(arg_string):
      comma = ", "
    return "%s%s" % (comma, arg_string)

  def MakeTypedOriginalArgString(self, prefix, add_comma = False):
    """Gets a list of arguments as they arg in GL."""
    args = self.GetOriginalArgs()
    arg_string = ", ".join(
        ["%s %s%s" % (arg.type, prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeOriginalArgString(self, prefix, add_comma = False):
    """Gets the list of arguments as they are in GL."""
    args = self.GetOriginalArgs()
    arg_string = ", ".join(
        ["%s%s" % (prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeTypedCmdArgString(self, prefix, add_comma = False):
    """Gets a typed list of arguments as they need to be for command buffers."""
    args = self.GetCmdArgs()
    arg_string = ", ".join(
        ["%s %s%s" % (arg.type, prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeCmdArgString(self, prefix, add_comma = False):
    """Gets the list of arguments as they need to be for command buffers."""
    args = self.GetCmdArgs()
    arg_string = ", ".join(
        ["%s%s" % (prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeTypedInitString(self, prefix, add_comma = False):
    """Gets a typed list of arguments as they need to be for cmd Init/Set."""
    args = self.GetInitArgs()
    arg_string = ", ".join(
        ["%s %s%s" % (arg.type, prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeInitString(self, prefix, add_comma = False):
    """Gets the list of arguments as they need to be for cmd Init/Set."""
    args = self.GetInitArgs()
    arg_string = ", ".join(
        ["%s%s" % (prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def WriteHandlerImplementation(self, file):
    """Writes the handler implementation for this command."""
    self.type_handler.WriteHandlerImplementation(self, file)

  def WriteValidationCode(self, file):
    """Writes the validation code for a command."""
    pass

  def WriteCmdArgFlag(self, file):
    """Writes the cmd kArgFlags constant."""
    file.Write("  static const cmd::ArgFlags kArgFlags = cmd::kFixed;\n")

  def WriteCmdComputeSize(self, file):
    """Writes the ComputeSize function for the command."""
    file.Write("  static uint32 ComputeSize() {\n")
    file.Write(
        "    return static_cast<uint32>(sizeof(ValueType));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteCmdSetHeader(self, file):
    """Writes the cmd's SetHeader function."""
    file.Write("  void SetHeader() {\n")
    file.Write("    header.SetCmd<ValueType>();\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteCmdInit(self, file):
    """Writes the cmd's Init function."""
    file.Write("  void Init(%s) {\n" % self.MakeTypedCmdArgString("_"))
    file.Write("    SetHeader();\n")
    args = self.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("  }\n")
    file.Write("\n")

  def WriteCmdSet(self, file):
    """Writes the cmd's Set function."""
    copy_args = self.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s) {\n" %
               self.MakeTypedCmdArgString("_", True))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s);\n" % copy_args)
    file.Write("    return NextCmdAddress<ValueType>(cmd);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteStruct(self, file):
    self.type_handler.WriteStruct(self, file)

  def WriteCmdHelper(self, file):
    """Writes the cmd's helper."""
    self.type_handler.WriteCmdHelper(self, file)

  def WriteServiceImplementation(self, file):
    """Writes the service implementation for a command."""
    self.type_handler.WriteServiceImplementation(self, file)

  def WriteGLES2ImplementationHeader(self, file):
    """Writes the GLES2 Implemention declaration."""
    self.type_handler.WriteGLES2ImplementationHeader(self, file)

  def WriteGLES2ImplementationImpl(self, file):
    """Writes the GLES2 Implemention definition."""
    self.type_handler.WriteGLES2ImplementationImpl(self, file)

  def WriteFormatTest(self, file):
    """Writes the cmd's format test."""
    self.type_handler.WriteFormatTest(self, file)


class ImmediateFunction(Function):
  """A class that represnets an immediate function command."""

  def __init__(self, func):
    new_args = []
    for arg in func.GetOriginalArgs():
      new_arg = arg.GetImmediateVersion()
      if new_arg:
        new_args.append(new_arg)

    cmd_args = []
    new_args_for_cmds = []
    for arg in func.args_for_cmds:
      new_arg = arg.GetImmediateVersion()
      if new_arg:
        new_args_for_cmds.append(new_arg)
        new_arg.AddCmdArgs(cmd_args)

    new_init_args = []
    for arg in new_args_for_cmds:
      arg.AddInitArgs(new_init_args)

    Function.__init__(
        self,
        "%sImmediate" % func.name,
        func.info,
        func.return_type,
        new_args,
        new_args_for_cmds,
        cmd_args,
        new_init_args,
        0)
    self.original_name = func.name

  def WriteServiceImplementation(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateServiceImplementation(self, file)

  def WriteHandlerImplementation(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateHandlerImplementation(self, file)

  def WriteValidationCode(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateValidationCode(self, file)

  def WriteCmdArgFlag(self, file):
    """Overridden from Function"""
    file.Write("  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;\n")

  def WriteCmdComputeSize(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdComputeSize(self, file)

  def WriteCmdSetHeader(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdSetHeader(self, file)

  def WriteCmdInit(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdInit(self, file)

  def WriteCmdSet(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdSet(self, file)

  def WriteCmdHelper(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdHelper(self, file)

  def WriteFormatTest(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateFormatTest(self, file)


def CreateArg(arg_string):
  """Creates an Argument."""
  arg_parts = arg_string.split()
  if len(arg_parts) == 1 and arg_parts[0] == 'void':
    return None
  # Is this a pointer argument?
  elif arg_string.find('*') >= 0:
    if arg_parts[0] == 'NonImmediate':
      return NonImmediatePointerArgument(
          arg_parts[-1],
          " ".join(arg_parts[1:-1]))
    else:
      return PointerArgument(
          arg_parts[-1],
          " ".join(arg_parts[0:-1]))
  # Is this a resource argument? Must come after pointer check.
  elif arg_parts[0] == 'GLResourceId':
    return ResourceIdArgument(
        arg_parts[-1],
        " ".join(arg_parts[0:-1]))
  else:
    return Argument(
        arg_parts[-1],
        " ".join(arg_parts[0:-1]))


class GLGenerator(object):
  """A class to generate GL command buffers."""

  _function_re = re.compile(r'GL_APICALL(.*?)GL_APIENTRY (.*?) \((.*?)\);')
  _non_alnum_re = re.compile(r'[^a-zA-Z0-9]')

  def __init__(self, verbose):
    self.original_functions = []
    self.functions = []
    self.verbose = verbose
    self._function_info = {}
    self._empty_type_handler = TypeHandler()
    self._empty_function_info = FunctionInfo({}, self._empty_type_handler)

    self._type_handlers = {
      'Create': CreateHandler(),
      'Custom': CustomHandler(),
      'Data': DataHandler(),
      'DELn': DELnHandler(),
      'GENn': GENnHandler(),
      'GETn': GETnHandler(),
      'GetGLchar': GetGLcharHandler(),
      'GLchar': GLcharHandler(),
      'HandWritten': HandWrittenHandler(),
      'Is': IsHandler(),
      'Manual': ManualHandler(),
      'PUT': PUTHandler(),
      'PUTn': PUTnHandler(),
      'STRn': STRnHandler(),
      'Todo': TodoHandler(),
    }

    for func_name in _FUNCTION_INFO:
      info = _FUNCTION_INFO[func_name]
      type = ''
      if 'type' in info:
        type = info['type']
      self._function_info[func_name] = FunctionInfo(info,
                                                    self.GetTypeHandler(type))

  def AddFunction(self, func):
    """Adds a function."""
    self.functions.append(func)

  def GetTypeHandler(self, name):
    """Gets a type info for the given type."""
    if name in self._type_handlers:
      return self._type_handlers[name]
    return self._empty_type_handler

  def GetFunctionInfo(self, name):
    """Gets a type info for the given function name."""
    if name in self._function_info:
      return self._function_info[name]
    return self._empty_function_info

  def Log(self, msg):
    """Prints something if verbose is true."""
    if self.verbose:
      print msg

  def WriteHeader(self, file):
    """Writes header to file"""
    file.Write(
        "// This file is auto-generated. DO NOT EDIT!\n"
        "\n")

  def WriteLicense(self, file):
    """Writes the license."""
    file.Write(_LICENSE)

  def WriteNamespaceOpen(self, file):
    """Writes the code for the namespace."""
    file.Write("namespace command_buffer {\n")
    file.Write("namespace gles2 {\n")
    file.Write("\n")

  def WriteNamespaceClose(self, file):
    """Writes the code to close the namespace."""
    file.Write("}  // namespace gles2\n")
    file.Write("}  // namespace command_buffer\n")
    file.Write("\n")

  def MakeGuard(self, filename):
    """Creates a header guard id."""
    base = os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
    hpath = os.path.abspath(filename)[len(base) + 1:]
    return self._non_alnum_re.sub('_', hpath).upper()

  def ParseArgs(self, arg_string):
    """Parses a function arg string."""
    args = []
    num_pointer_args = 0
    parts = arg_string.split(',')
    for arg_string in parts:
      arg = CreateArg(arg_string)
      if arg:
        args.append(arg)
        if arg.IsPointer():
          num_pointer_args += 1
    return (args, num_pointer_args)

  def ParseGLH(self, filename):
    """Parses the GL2.h file and extracts the functions"""
    for line in _GL_FUNCTIONS.splitlines():
      match = self._function_re.match(line)
      if match:
        func_name = match.group(2)[2:]
        func_info = self.GetFunctionInfo(func_name)
        if func_info.type != 'Noop':
          return_type = match.group(1).strip()
          arg_string = match.group(3)
          (args, num_pointer_args) = self.ParseArgs(arg_string)
          args_for_cmds = args
          if hasattr(func_info, 'cmd_args'):
            (args_for_cmds, num_pointer_args) = (
                self.ParseArgs(getattr(func_info, 'cmd_args')))
          cmd_args = []
          for arg in args_for_cmds:
            arg.AddCmdArgs(cmd_args)
          init_args = []
          for arg in args_for_cmds:
            arg.AddInitArgs(init_args)
          return_arg = CreateArg(return_type + " result")
          if return_arg:
            init_args.append(return_arg)
          f = Function(func_name, func_info, return_type, args, args_for_cmds,
                       cmd_args, init_args, num_pointer_args)
          self.original_functions.append(f)
          self.AddFunction(f)
          f.type_handler.AddImmediateFunction(self, f)

    self.Log("Auto Generated Functions    : %d" %
             len([f for f in self.functions if f.can_auto_generate or
                  (not f.IsType('') and not f.IsType('Custom') and
                   not f.IsType('Todo'))]))

    funcs = [f for f in self.functions if not f.can_auto_generate and
             (f.IsType('') or f.IsType('Custom') or f.IsType('Todo'))]
    self.Log("Non Auto Generated Functions: %d" % len(funcs))

    for f in funcs:
      self.Log("  %-10s %-20s gl%s" % (f.info.type, f.return_type, f.name))

  def WriteCommandIds(self, filename):
    """Writes the command buffer format"""
    file = CWriter(filename)
    self.WriteHeader(file)
    file.Write("#define GLES2_COMMAND_LIST(OP) \\\n")
    command_id = _FIRST_SPECIFIC_COMMAND_ID
    for func in self.functions:
      file.Write("  %-60s /* %d */ \\\n" % ("OP(%s)" % func.name, command_id))
      command_id += 1
    file.Write("\n")

    file.Write("enum CommandId {\n")
    file.Write("  kStartPoint = cmd::kLastCommonId,  "
               "// All GLES2 commands start after this.\n")
    file.Write("#define GLES2_CMD_OP(name) k ## name,\n")
    file.Write("  GLES2_COMMAND_LIST(GLES2_CMD_OP)\n")
    file.Write("#undef GLES2_CMD_OP\n")
    file.Write("  kNumCommands\n")
    file.Write("};\n")
    file.Write("\n")
    file.Close()

  def WriteFormat(self, filename):
    """Writes the command buffer format"""
    file = CWriter(filename)
    self.WriteHeader(file)
    file.Write("#pragma pack(push, 1)\n")
    file.Write("\n")

    for func in self.functions:
      func.WriteStruct(file)

    file.Write("#pragma pack(pop)\n")
    file.Write("\n")
    file.Close()

  def WriteFormatTest(self, filename):
    """Writes the command buffer format test."""
    file = CWriter(filename)
    self.WriteHeader(file)
    file.Write("// This file contains unit tests for gles2 commmands\n")
    file.Write("// It is included by gles2_cmd_format_test.cc\n")
    file.Write("\n")

    for func in self.functions:
      func.WriteFormatTest(file)

    file.Close()

  def WriteCommandIdTest(self, filename):
    """Writes the command id test."""
    file = CWriter(filename)
    self.WriteLicense(file)
    file.Write("// This file contains unit tests for gles2 commmand ids\n")
    file.Write("\n")
    file.Write("#include \"tests/common/win/testing_common.h\"\n")
    file.Write("#include \"gpu/command_buffer/common/gles2_cmd_format.h\"\n")
    file.Write("\n")

    self.WriteNamespaceOpen(file)

    file.Write("// *** These IDs MUST NOT CHANGE!!! ***\n")
    file.Write("// Changing them will break all client programs.\n")
    file.Write("TEST(GLES2CommandIdTest, CommandIdsMatch) {\n")
    command_id = 1024
    for func in self.functions:
      file.Write("  COMPILE_ASSERT(%s::kCmdId == %d,\n" %
                 (func.name, command_id))
      file.Write("                 GLES2_%s_kCmdId_mismatch);\n" % func.name)
      command_id += 1

    file.Write("}\n")

    self.WriteNamespaceClose(file)

    file.Close()

  def WriteCmdHelperHeader(self, filename):
    """Writes the gles2 command helper."""
    file = CWriter(filename)

    for func in self.functions:
      func.WriteCmdHelper(file)

    file.Close()

  def WriteServiceImplementation(self, filename):
    """Writes the service decorder implementation."""
    file = CWriter(filename)
    self.WriteHeader(file)
    file.Write("// It is included by gles2_cmd_decoder.cc\n")
    file.Write("\n")

    for func in self.functions:
      func.WriteServiceImplementation(file)

    file.Close()

  def WriteServiceValidation(self, filename):
    file = CWriter(filename)
    self.WriteLicense(file)
    file.Write("\n")
    self.WriteNamespaceOpen(file)
    file.Write("namespace {\n")
    file.Write("\n")
    for func in self.functions:
      file.Write("parse_error::ParseError Validate%s(\n" % func.name)
      file.Write(
          "    GLES2Decoder* decoder, uint32 immediate_data_size%s) {\n" %
          func.MakeTypedOriginalArgString("", True))
      for arg in func.GetOriginalArgs():
        arg.WriteValidationCode(file)
      func.WriteValidationCode(file)
      file.Write("  return parse_error::kParseNoError;\n")
      file.Write("}\n")
    file.Write("}  // anonymous namespace\n")
    self.WriteNamespaceClose(file)
    file.Close()

  def WriteGLES2CLibImplementation(self, filename):
    """Writes the GLES2 c lib implementation."""
    file = CWriter(filename)
    self.WriteHeader(file)
    file.Write("\n")
    file.Write("// These functions emluate GLES2 over command buffers.\n")
    file.Write("\n")
    file.Write("\n")

    for func in self.original_functions:
      file.Write("%s GLES2%s(%s) {\n" %
                 (func.return_type, func.name,
                  func.MakeTypedOriginalArgString("")))
      return_string = "return "
      if func.return_type == "void":
        return_string = ""
      file.Write("  %sgles2::GetGLContext()->%s(%s);\n" %
                 (return_string, func.original_name,
                  func.MakeOriginalArgString("")))
      file.Write("}\n")

    file.Write("\n")

    file.Close()

  def WriteGLES2ImplementationHeader(self, filename):
    """Writes the GLES2 helper header."""
    file = CWriter(filename)
    self.WriteHeader(file)
    file.Write(
        "// This file is included by gles2_implementation.h to declare the\n")
    file.Write("// GL api functions.\n")
    for func in self.original_functions:
      func.WriteGLES2ImplementationHeader(file)
    file.Close()

  def WriteGLES2ImplementationImpl(self, filename):
    """Writes the gles2 helper implementation."""
    file = CWriter(filename)
    self.WriteLicense(file)
    file.Write("\n")
    file.Write("// A class to emluate GLES2 over command buffers.\n")
    file.Write("\n")
    file.Write(
        "#include \"gpu/command_buffer/client/gles2_implementation.h\"\n")
    file.Write("\n")
    self.WriteNamespaceOpen(file)
    for func in self.original_functions:
      func.WriteGLES2ImplementationImpl(file)
    file.Write("\n")

    self.WriteNamespaceClose(file)
    file.Close()


def main(argv):
  """This is the main function."""
  parser = OptionParser()
  parser.add_option(
      "-g", "--generate-implementation-templates", action="store_true",
      help="generates files that are generally hand edited..")
  parser.add_option(
      "--generate-command-id-tests", action="store_true",
      help="generate tests for commands ids. Commands MUST not change ID!")
  parser.add_option(
      "-v", "--verbose", action="store_true",
      help="prints more output.")

  (options, args) = parser.parse_args(args=argv)

  gen = GLGenerator(options.verbose)
  gen.ParseGLH("common/GLES2/gl2.h")
  gen.WriteCommandIds("common/gles2_cmd_ids_autogen.h")
  gen.WriteFormat("common/gles2_cmd_format_autogen.h")
  gen.WriteFormatTest("common/gles2_cmd_format_test_autogen.h")
  gen.WriteGLES2ImplementationHeader("client/gles2_implementation_autogen.h")
  gen.WriteGLES2CLibImplementation("client/gles2_c_lib_autogen.h")
  gen.WriteCmdHelperHeader("client/gles2_cmd_helper_autogen.h")
  gen.WriteServiceImplementation("service/gles2_cmd_decoder_autogen.h")

  if options.generate_implementation_templates:
    gen.WriteGLES2ImplementationImpl("client/gles2_implementation_gen.h")
    gen.WriteServiceValidation("service/gles2_cmd_decoder_validate.h")

  if options.generate_command_id_tests:
    gen.WriteCommandIdTest("common/gles2_cmd_id_test.cc")


if __name__ == '__main__':
  main(sys.argv[1:])

