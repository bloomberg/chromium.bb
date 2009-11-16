// This file is auto-generated. DO NOT EDIT!


// These functions emluate GLES2 over command buffers.


inline void glActiveTexture(GLenum texture) {
  g_gl_impl->ActiveTexture(texture);
}
inline void glAttachShader(GLuint program, GLuint shader) {
  g_gl_impl->AttachShader(program, shader);
}
inline void glBindAttribLocation(
    GLuint program, GLuint index, const char* name) {
  g_gl_impl->BindAttribLocation(program, index, name);
}
inline void glBindBuffer(GLenum target, GLuint buffer) {
  g_gl_impl->BindBuffer(target, buffer);
}
inline void glBindFramebuffer(GLenum target, GLuint framebuffer) {
  g_gl_impl->BindFramebuffer(target, framebuffer);
}
inline void glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
  g_gl_impl->BindRenderbuffer(target, renderbuffer);
}
inline void glBindTexture(GLenum target, GLuint texture) {
  g_gl_impl->BindTexture(target, texture);
}
inline void glBlendColor(
    GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  g_gl_impl->BlendColor(red, green, blue, alpha);
}
inline void glBlendEquation(GLenum mode) {
  g_gl_impl->BlendEquation(mode);
}
inline void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
  g_gl_impl->BlendEquationSeparate(modeRGB, modeAlpha);
}
inline void glBlendFunc(GLenum sfactor, GLenum dfactor) {
  g_gl_impl->BlendFunc(sfactor, dfactor);
}
inline void glBlendFuncSeparate(
    GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
  g_gl_impl->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}
inline void glBufferData(
    GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
  g_gl_impl->BufferData(target, size, data, usage);
}
inline void glBufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
  g_gl_impl->BufferSubData(target, offset, size, data);
}
inline GLenum glCheckFramebufferStatus(GLenum target) {
  return g_gl_impl->CheckFramebufferStatus(target);
}
inline void glClear(GLbitfield mask) {
  g_gl_impl->Clear(mask);
}
inline void glClearColor(
    GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  g_gl_impl->ClearColor(red, green, blue, alpha);
}
inline void glClearDepthf(GLclampf depth) {
  g_gl_impl->ClearDepthf(depth);
}
inline void glClearStencil(GLint s) {
  g_gl_impl->ClearStencil(s);
}
inline void glColorMask(
    GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
  g_gl_impl->ColorMask(red, green, blue, alpha);
}
inline void glCompileShader(GLuint shader) {
  g_gl_impl->CompileShader(shader);
}
inline void glCompressedTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLsizei width,
    GLsizei height, GLint border, GLsizei imageSize, const void* data) {
  g_gl_impl->CompressedTexImage2D(
      target, level, internalformat, width, height, border, imageSize, data);
}
inline void glCompressedTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
  g_gl_impl->CompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}
inline void glCopyTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
    GLsizei width, GLsizei height, GLint border) {
  g_gl_impl->CopyTexImage2D(
      target, level, internalformat, x, y, width, height, border);
}
inline void glCopyTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
    GLsizei width, GLsizei height) {
  g_gl_impl->CopyTexSubImage2D(
      target, level, xoffset, yoffset, x, y, width, height);
}
inline GLuint glCreateProgram() {
  return g_gl_impl->CreateProgram();
}
inline GLuint glCreateShader(GLenum type) {
  return g_gl_impl->CreateShader(type);
}
inline void glCullFace(GLenum mode) {
  g_gl_impl->CullFace(mode);
}
inline void glDeleteBuffers(GLsizei n, const GLuint* buffers) {
  g_gl_impl->DeleteBuffers(n, buffers);
}
inline void glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  g_gl_impl->DeleteFramebuffers(n, framebuffers);
}
inline void glDeleteProgram(GLuint program) {
  g_gl_impl->DeleteProgram(program);
}
inline void glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  g_gl_impl->DeleteRenderbuffers(n, renderbuffers);
}
inline void glDeleteShader(GLuint shader) {
  g_gl_impl->DeleteShader(shader);
}
inline void glDeleteTextures(GLsizei n, const GLuint* textures) {
  g_gl_impl->DeleteTextures(n, textures);
}
inline void glDepthFunc(GLenum func) {
  g_gl_impl->DepthFunc(func);
}
inline void glDepthMask(GLboolean flag) {
  g_gl_impl->DepthMask(flag);
}
inline void glDepthRangef(GLclampf zNear, GLclampf zFar) {
  g_gl_impl->DepthRangef(zNear, zFar);
}
inline void glDetachShader(GLuint program, GLuint shader) {
  g_gl_impl->DetachShader(program, shader);
}
inline void glDisable(GLenum cap) {
  g_gl_impl->Disable(cap);
}
inline void glDisableVertexAttribArray(GLuint index) {
  g_gl_impl->DisableVertexAttribArray(index);
}
inline void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  g_gl_impl->DrawArrays(mode, first, count);
}
inline void glDrawElements(
    GLenum mode, GLsizei count, GLenum type, const void* indices) {
  g_gl_impl->DrawElements(mode, count, type, indices);
}
inline void glEnable(GLenum cap) {
  g_gl_impl->Enable(cap);
}
inline void glEnableVertexAttribArray(GLuint index) {
  g_gl_impl->EnableVertexAttribArray(index);
}
inline void glFinish() {
  g_gl_impl->Finish();
}
inline void glFlush() {
  g_gl_impl->Flush();
}
inline void glFramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint renderbuffer) {
  g_gl_impl->FramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
}
inline void glFramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
    GLint level) {
  g_gl_impl->FramebufferTexture2D(
      target, attachment, textarget, texture, level);
}
inline void glFrontFace(GLenum mode) {
  g_gl_impl->FrontFace(mode);
}
inline void glGenBuffers(GLsizei n, GLuint* buffers) {
  g_gl_impl->GenBuffers(n, buffers);
}
inline void glGenerateMipmap(GLenum target) {
  g_gl_impl->GenerateMipmap(target);
}
inline void glGenFramebuffers(GLsizei n, GLuint* framebuffers) {
  g_gl_impl->GenFramebuffers(n, framebuffers);
}
inline void glGenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  g_gl_impl->GenRenderbuffers(n, renderbuffers);
}
inline void glGenTextures(GLsizei n, GLuint* textures) {
  g_gl_impl->GenTextures(n, textures);
}
inline void glGetActiveAttrib(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  g_gl_impl->GetActiveAttrib(
      program, index, bufsize, length, size, type, name);
}
inline void glGetActiveUniform(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  g_gl_impl->GetActiveUniform(
      program, index, bufsize, length, size, type, name);
}
inline void glGetAttachedShaders(
    GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) {
  g_gl_impl->GetAttachedShaders(program, maxcount, count, shaders);
}
inline int glGetAttribLocation(GLuint program, const char* name) {
  return g_gl_impl->GetAttribLocation(program, name);
}
inline void glGetBooleanv(GLenum pname, GLboolean* params) {
  g_gl_impl->GetBooleanv(pname, params);
}
inline void glGetBufferParameteriv(
    GLenum target, GLenum pname, GLint* params) {
  g_gl_impl->GetBufferParameteriv(target, pname, params);
}
inline GLenum glGetError() {
  return g_gl_impl->GetError();
}
inline void glGetFloatv(GLenum pname, GLfloat* params) {
  g_gl_impl->GetFloatv(pname, params);
}
inline void glGetFramebufferAttachmentParameteriv(
    GLenum target, GLenum attachment, GLenum pname, GLint* params) {
  g_gl_impl->GetFramebufferAttachmentParameteriv(
      target, attachment, pname, params);
}
inline void glGetIntegerv(GLenum pname, GLint* params) {
  g_gl_impl->GetIntegerv(pname, params);
}
inline void glGetProgramiv(GLuint program, GLenum pname, GLint* params) {
  g_gl_impl->GetProgramiv(program, pname, params);
}
inline void glGetProgramInfoLog(
    GLuint program, GLsizei bufsize, GLsizei* length, char* infolog) {
  g_gl_impl->GetProgramInfoLog(program, bufsize, length, infolog);
}
inline void glGetRenderbufferParameteriv(
    GLenum target, GLenum pname, GLint* params) {
  g_gl_impl->GetRenderbufferParameteriv(target, pname, params);
}
inline void glGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  g_gl_impl->GetShaderiv(shader, pname, params);
}
inline void glGetShaderInfoLog(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog) {
  g_gl_impl->GetShaderInfoLog(shader, bufsize, length, infolog);
}
inline void glGetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
  g_gl_impl->GetShaderPrecisionFormat(
      shadertype, precisiontype, range, precision);
}
inline void glGetShaderSource(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* source) {
  g_gl_impl->GetShaderSource(shader, bufsize, length, source);
}
inline const GLubyte* glGetString(GLenum name) {
  return g_gl_impl->GetString(name);
}
inline void glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
  g_gl_impl->GetTexParameterfv(target, pname, params);
}
inline void glGetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
  g_gl_impl->GetTexParameteriv(target, pname, params);
}
inline void glGetUniformfv(GLuint program, GLint location, GLfloat* params) {
  g_gl_impl->GetUniformfv(program, location, params);
}
inline void glGetUniformiv(GLuint program, GLint location, GLint* params) {
  g_gl_impl->GetUniformiv(program, location, params);
}
inline int glGetUniformLocation(GLuint program, const char* name) {
  return g_gl_impl->GetUniformLocation(program, name);
}
inline void glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params) {
  g_gl_impl->GetVertexAttribfv(index, pname, params);
}
inline void glGetVertexAttribiv(GLuint index, GLenum pname, GLint* params) {
  g_gl_impl->GetVertexAttribiv(index, pname, params);
}
inline void glGetVertexAttribPointerv(
    GLuint index, GLenum pname, void** pointer) {
  g_gl_impl->GetVertexAttribPointerv(index, pname, pointer);
}
inline void glHint(GLenum target, GLenum mode) {
  g_gl_impl->Hint(target, mode);
}
inline GLboolean glIsBuffer(GLuint buffer) {
  return g_gl_impl->IsBuffer(buffer);
}
inline GLboolean glIsEnabled(GLenum cap) {
  return g_gl_impl->IsEnabled(cap);
}
inline GLboolean glIsFramebuffer(GLuint framebuffer) {
  return g_gl_impl->IsFramebuffer(framebuffer);
}
inline GLboolean glIsProgram(GLuint program) {
  return g_gl_impl->IsProgram(program);
}
inline GLboolean glIsRenderbuffer(GLuint renderbuffer) {
  return g_gl_impl->IsRenderbuffer(renderbuffer);
}
inline GLboolean glIsShader(GLuint shader) {
  return g_gl_impl->IsShader(shader);
}
inline GLboolean glIsTexture(GLuint texture) {
  return g_gl_impl->IsTexture(texture);
}
inline void glLineWidth(GLfloat width) {
  g_gl_impl->LineWidth(width);
}
inline void glLinkProgram(GLuint program) {
  g_gl_impl->LinkProgram(program);
}
inline void glPixelStorei(GLenum pname, GLint param) {
  g_gl_impl->PixelStorei(pname, param);
}
inline void glPolygonOffset(GLfloat factor, GLfloat units) {
  g_gl_impl->PolygonOffset(factor, units);
}
inline void glReadPixels(
    GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
    void* pixels) {
  g_gl_impl->ReadPixels(x, y, width, height, format, type, pixels);
}
inline void glRenderbufferStorage(
    GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  g_gl_impl->RenderbufferStorage(target, internalformat, width, height);
}
inline void glSampleCoverage(GLclampf value, GLboolean invert) {
  g_gl_impl->SampleCoverage(value, invert);
}
inline void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  g_gl_impl->Scissor(x, y, width, height);
}
inline void glShaderSource(
    GLuint shader, GLsizei count, const char** string, const GLint* length) {
  g_gl_impl->ShaderSource(shader, count, string, length);
}
inline void glStencilFunc(GLenum func, GLint ref, GLuint mask) {
  g_gl_impl->StencilFunc(func, ref, mask);
}
inline void glStencilFuncSeparate(
    GLenum face, GLenum func, GLint ref, GLuint mask) {
  g_gl_impl->StencilFuncSeparate(face, func, ref, mask);
}
inline void glStencilMask(GLuint mask) {
  g_gl_impl->StencilMask(mask);
}
inline void glStencilMaskSeparate(GLenum face, GLuint mask) {
  g_gl_impl->StencilMaskSeparate(face, mask);
}
inline void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  g_gl_impl->StencilOp(fail, zfail, zpass);
}
inline void glStencilOpSeparate(
    GLenum face, GLenum fail, GLenum zfail, GLenum zpass) {
  g_gl_impl->StencilOpSeparate(face, fail, zfail, zpass);
}
inline void glTexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  g_gl_impl->TexImage2D(
      target, level, internalformat, width, height, border, format, type,
      pixels);
}
inline void glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
  g_gl_impl->TexParameterf(target, pname, param);
}
inline void glTexParameterfv(
    GLenum target, GLenum pname, const GLfloat* params) {
  g_gl_impl->TexParameterfv(target, pname, params);
}
inline void glTexParameteri(GLenum target, GLenum pname, GLint param) {
  g_gl_impl->TexParameteri(target, pname, param);
}
inline void glTexParameteriv(
    GLenum target, GLenum pname, const GLint* params) {
  g_gl_impl->TexParameteriv(target, pname, params);
}
inline void glTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels) {
  g_gl_impl->TexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
}
inline void glUniform1f(GLint location, GLfloat x) {
  g_gl_impl->Uniform1f(location, x);
}
inline void glUniform1fv(GLint location, GLsizei count, const GLfloat* v) {
  g_gl_impl->Uniform1fv(location, count, v);
}
inline void glUniform1i(GLint location, GLint x) {
  g_gl_impl->Uniform1i(location, x);
}
inline void glUniform1iv(GLint location, GLsizei count, const GLint* v) {
  g_gl_impl->Uniform1iv(location, count, v);
}
inline void glUniform2f(GLint location, GLfloat x, GLfloat y) {
  g_gl_impl->Uniform2f(location, x, y);
}
inline void glUniform2fv(GLint location, GLsizei count, const GLfloat* v) {
  g_gl_impl->Uniform2fv(location, count, v);
}
inline void glUniform2i(GLint location, GLint x, GLint y) {
  g_gl_impl->Uniform2i(location, x, y);
}
inline void glUniform2iv(GLint location, GLsizei count, const GLint* v) {
  g_gl_impl->Uniform2iv(location, count, v);
}
inline void glUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  g_gl_impl->Uniform3f(location, x, y, z);
}
inline void glUniform3fv(GLint location, GLsizei count, const GLfloat* v) {
  g_gl_impl->Uniform3fv(location, count, v);
}
inline void glUniform3i(GLint location, GLint x, GLint y, GLint z) {
  g_gl_impl->Uniform3i(location, x, y, z);
}
inline void glUniform3iv(GLint location, GLsizei count, const GLint* v) {
  g_gl_impl->Uniform3iv(location, count, v);
}
inline void glUniform4f(
    GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  g_gl_impl->Uniform4f(location, x, y, z, w);
}
inline void glUniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  g_gl_impl->Uniform4fv(location, count, v);
}
inline void glUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
  g_gl_impl->Uniform4i(location, x, y, z, w);
}
inline void glUniform4iv(GLint location, GLsizei count, const GLint* v) {
  g_gl_impl->Uniform4iv(location, count, v);
}
inline void glUniformMatrix2fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  g_gl_impl->UniformMatrix2fv(location, count, transpose, value);
}
inline void glUniformMatrix3fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  g_gl_impl->UniformMatrix3fv(location, count, transpose, value);
}
inline void glUniformMatrix4fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  g_gl_impl->UniformMatrix4fv(location, count, transpose, value);
}
inline void glUseProgram(GLuint program) {
  g_gl_impl->UseProgram(program);
}
inline void glValidateProgram(GLuint program) {
  g_gl_impl->ValidateProgram(program);
}
inline void glVertexAttrib1f(GLuint indx, GLfloat x) {
  g_gl_impl->VertexAttrib1f(indx, x);
}
inline void glVertexAttrib1fv(GLuint indx, const GLfloat* values) {
  g_gl_impl->VertexAttrib1fv(indx, values);
}
inline void glVertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
  g_gl_impl->VertexAttrib2f(indx, x, y);
}
inline void glVertexAttrib2fv(GLuint indx, const GLfloat* values) {
  g_gl_impl->VertexAttrib2fv(indx, values);
}
inline void glVertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  g_gl_impl->VertexAttrib3f(indx, x, y, z);
}
inline void glVertexAttrib3fv(GLuint indx, const GLfloat* values) {
  g_gl_impl->VertexAttrib3fv(indx, values);
}
inline void glVertexAttrib4f(
    GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  g_gl_impl->VertexAttrib4f(indx, x, y, z, w);
}
inline void glVertexAttrib4fv(GLuint indx, const GLfloat* values) {
  g_gl_impl->VertexAttrib4fv(indx, values);
}
inline void glVertexAttribPointer(
    GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void* ptr) {
  g_gl_impl->VertexAttribPointer(indx, size, type, normalized, stride, ptr);
}
inline void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  g_gl_impl->Viewport(x, y, width, height);
}

