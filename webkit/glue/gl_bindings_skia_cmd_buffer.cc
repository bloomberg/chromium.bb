// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "webkit/glue/gl_bindings_skia_cmd_buffer.h"

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include "gpu/GLES2/gl2.h"
#include "gpu/GLES2/gl2ext.h"

#include "third_party/skia/gpu/include/GrGLInterface.h"

namespace webkit_glue {

GrGLInterface* CreateCommandBufferSkiaGLBinding() {
  GrGLInterface* gl_interface = new GrGLInterface;
  gl_interface->fBindingsExported = kES2_GrGLBinding;
  gl_interface->fActiveTexture = glActiveTexture;
  gl_interface->fAttachShader = glAttachShader;
  gl_interface->fBindAttribLocation = glBindAttribLocation;
  gl_interface->fBindBuffer = glBindBuffer;
  gl_interface->fBindTexture = glBindTexture;
  gl_interface->fBlendColor = glBlendColor;
  gl_interface->fBlendFunc = glBlendFunc;
  gl_interface->fBufferData = glBufferData;
  gl_interface->fBufferSubData = glBufferSubData;
  gl_interface->fClear = glClear;
  gl_interface->fClearColor = glClearColor;
  gl_interface->fClearStencil = glClearStencil;
  gl_interface->fColorMask = glColorMask;
  gl_interface->fCompileShader = glCompileShader;
  gl_interface->fCompressedTexImage2D = glCompressedTexImage2D;
  gl_interface->fCreateProgram = glCreateProgram;
  gl_interface->fCreateShader = glCreateShader;
  gl_interface->fCullFace = glCullFace;
  gl_interface->fDeleteBuffers = glDeleteBuffers;
  gl_interface->fDeleteProgram = glDeleteProgram;
  gl_interface->fDeleteShader = glDeleteShader;
  gl_interface->fDeleteTextures = glDeleteTextures;
  gl_interface->fDepthMask = glDepthMask;
  gl_interface->fDisable = glDisable;
  gl_interface->fDisableVertexAttribArray = glDisableVertexAttribArray;
  gl_interface->fDrawArrays = glDrawArrays;
  gl_interface->fDrawElements = glDrawElements;
  gl_interface->fEnable = glEnable;
  gl_interface->fEnableVertexAttribArray = glEnableVertexAttribArray;
  gl_interface->fFrontFace = glFrontFace;
  gl_interface->fGenBuffers = glGenBuffers;
  gl_interface->fGenTextures = glGenTextures;
  gl_interface->fGetBufferParameteriv = glGetBufferParameteriv;
  gl_interface->fGetError = glGetError;
  gl_interface->fGetIntegerv = glGetIntegerv;
  gl_interface->fGetProgramInfoLog = glGetProgramInfoLog;
  gl_interface->fGetProgramiv = glGetProgramiv;
  gl_interface->fGetShaderInfoLog = glGetShaderInfoLog;
  gl_interface->fGetShaderiv = glGetShaderiv;
  gl_interface->fGetString = glGetString;
  gl_interface->fGetUniformLocation = glGetUniformLocation;
  gl_interface->fLineWidth = glLineWidth;
  gl_interface->fLinkProgram = glLinkProgram;
  gl_interface->fPixelStorei = glPixelStorei;
  gl_interface->fReadPixels = glReadPixels;
  gl_interface->fScissor = glScissor;
  gl_interface->fShaderSource = glShaderSource;
  gl_interface->fStencilFunc = glStencilFunc;
  gl_interface->fStencilFuncSeparate = glStencilFuncSeparate;
  gl_interface->fStencilMask = glStencilMask;
  gl_interface->fStencilMaskSeparate = glStencilMaskSeparate;
  gl_interface->fStencilOp = glStencilOp;
  gl_interface->fStencilOpSeparate = glStencilOpSeparate;
  gl_interface->fTexImage2D = glTexImage2D;
  gl_interface->fTexParameteri = glTexParameteri;
  gl_interface->fTexSubImage2D = glTexSubImage2D;
  gl_interface->fUniform1f = glUniform1f;
  gl_interface->fUniform1i = glUniform1i;
  gl_interface->fUniform1fv = glUniform1fv;
  gl_interface->fUniform1iv = glUniform1iv;
  gl_interface->fUniform2f = glUniform2f;
  gl_interface->fUniform2i = glUniform2i;
  gl_interface->fUniform2fv = glUniform2fv;
  gl_interface->fUniform2iv = glUniform2iv;
  gl_interface->fUniform3f = glUniform3f;
  gl_interface->fUniform3i = glUniform3i;
  gl_interface->fUniform3fv = glUniform3fv;
  gl_interface->fUniform3iv = glUniform3iv;
  gl_interface->fUniform4f = glUniform4f;
  gl_interface->fUniform4i = glUniform4i;
  gl_interface->fUniform4fv = glUniform4fv;
  gl_interface->fUniform4iv = glUniform4iv;
  gl_interface->fUniformMatrix2fv = glUniformMatrix2fv;
  gl_interface->fUniformMatrix3fv = glUniformMatrix3fv;
  gl_interface->fUniformMatrix4fv = glUniformMatrix4fv;
  gl_interface->fUseProgram = glUseProgram;
  gl_interface->fVertexAttrib4fv = glVertexAttrib4fv;
  gl_interface->fVertexAttribPointer = glVertexAttribPointer;
  gl_interface->fViewport = glViewport;
  gl_interface->fBindFramebuffer = glBindFramebuffer;
  gl_interface->fBindRenderbuffer = glBindRenderbuffer;
  gl_interface->fCheckFramebufferStatus = glCheckFramebufferStatus;
  gl_interface->fDeleteFramebuffers = glDeleteFramebuffers;
  gl_interface->fDeleteRenderbuffers = glDeleteRenderbuffers;
  gl_interface->fFramebufferRenderbuffer = glFramebufferRenderbuffer;
  gl_interface->fFramebufferTexture2D = glFramebufferTexture2D;
  gl_interface->fGenFramebuffers = glGenFramebuffers;
  gl_interface->fGenRenderbuffers = glGenRenderbuffers;
  gl_interface->fGetFramebufferAttachmentParameteriv =
    glGetFramebufferAttachmentParameteriv;
  gl_interface->fGetRenderbufferParameteriv = glGetRenderbufferParameteriv;
  gl_interface->fRenderbufferStorage = glRenderbufferStorage;
  gl_interface->fRenderbufferStorageMultisample =
    glRenderbufferStorageMultisampleEXT;
  gl_interface->fBlitFramebuffer = glBlitFramebufferEXT;
  return gl_interface;
}

}  // namespace webkit_glue

