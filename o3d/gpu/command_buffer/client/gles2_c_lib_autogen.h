// This file is auto-generated. DO NOT EDIT!


// These functions emluate GLES2 over command buffers.


void glActiveTexture(GLenum texture) {
  g_gl_impl->ActiveTexture(texture);
}
void glAttachShader(GLuint program, GLuint shader) {
  g_gl_impl->AttachShader(program, shader);
}
void glBindAttribLocation(GLuint program, GLuint index, const char* name) {
  g_gl_impl->BindAttribLocation(program, index, name);
}
void glBindBuffer(GLenum target, GLuint buffer) {
  g_gl_impl->BindBuffer(target, buffer);
}
void glBindFramebuffer(GLenum target, GLuint framebuffer) {
  g_gl_impl->BindFramebuffer(target, framebuffer);
}
void glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
  g_gl_impl->BindRenderbuffer(target, renderbuffer);
}
void glBindTexture(GLenum target, GLuint texture) {
  g_gl_impl->BindTexture(target, texture);
}
void glBlendColor(
    GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  g_gl_impl->BlendColor(red, green, blue, alpha);
}
void glBlendEquation(GLenum mode) {
  g_gl_impl->BlendEquation(mode);
}
void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
  g_gl_impl->BlendEquationSeparate(modeRGB, modeAlpha);
}
void glBlendFunc(GLenum sfactor, GLenum dfactor) {
  g_gl_impl->BlendFunc(sfactor, dfactor);
}
void glBlendFuncSeparate(
    GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
  g_gl_impl->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}
void glBufferData(
    GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
  g_gl_impl->BufferData(target, size, data, usage);
}
void glBufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
  g_gl_impl->BufferSubData(target, offset, size, data);
}
GLenum glCheckFramebufferStatus(GLenum target) {
  return g_gl_impl->CheckFramebufferStatus(target);
}
void glClear(GLbitfield mask) {
  g_gl_impl->Clear(mask);
}
void glClearColor(
    GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  g_gl_impl->ClearColor(red, green, blue, alpha);
}
void glClearDepthf(GLclampf depth) {
  g_gl_impl->ClearDepthf(depth);
}
void glClearStencil(GLint s) {
  g_gl_impl->ClearStencil(s);
}
void glColorMask(
    GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
  g_gl_impl->ColorMask(red, green, blue, alpha);
}
void glCompileShader(GLuint shader) {
  g_gl_impl->CompileShader(shader);
}
void glCompressedTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLsizei width,
    GLsizei height, GLint border, GLsizei imageSize, const void* data) {
  g_gl_impl->CompressedTexImage2D(
      target, level, internalformat, width, height, border, imageSize, data);
}
void glCompressedTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
  g_gl_impl->CompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}
void glCopyTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
    GLsizei width, GLsizei height, GLint border) {
  g_gl_impl->CopyTexImage2D(
      target, level, internalformat, x, y, width, height, border);
}
void glCopyTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
    GLsizei width, GLsizei height) {
  g_gl_impl->CopyTexSubImage2D(
      target, level, xoffset, yoffset, x, y, width, height);
}
GLuint glCreateProgram() {
  return g_gl_impl->CreateProgram();
}
GLuint glCreateShader(GLenum type) {
  return g_gl_impl->CreateShader(type);
}
void glCullFace(GLenum mode) {
  g_gl_impl->CullFace(mode);
}
void glDeleteBuffers(GLsizei n, const GLuint* buffers) {
  g_gl_impl->DeleteBuffers(n, buffers);
}
void glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  g_gl_impl->DeleteFramebuffers(n, framebuffers);
}
void glDeleteProgram(GLuint program) {
  g_gl_impl->DeleteProgram(program);
}
void glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  g_gl_impl->DeleteRenderbuffers(n, renderbuffers);
}
void glDeleteShader(GLuint shader) {
  g_gl_impl->DeleteShader(shader);
}
void glDeleteTextures(GLsizei n, const GLuint* textures) {
  g_gl_impl->DeleteTextures(n, textures);
}
void glDepthFunc(GLenum func) {
  g_gl_impl->DepthFunc(func);
}
void glDepthMask(GLboolean flag) {
  g_gl_impl->DepthMask(flag);
}
void glDepthRangef(GLclampf zNear, GLclampf zFar) {
  g_gl_impl->DepthRangef(zNear, zFar);
}
void glDetachShader(GLuint program, GLuint shader) {
  g_gl_impl->DetachShader(program, shader);
}
void glDisable(GLenum cap) {
  g_gl_impl->Disable(cap);
}
void glDisableVertexAttribArray(GLuint index) {
  g_gl_impl->DisableVertexAttribArray(index);
}
void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  g_gl_impl->DrawArrays(mode, first, count);
}
void glDrawElements(
    GLenum mode, GLsizei count, GLenum type, const void* indices) {
  g_gl_impl->DrawElements(mode, count, type, indices);
}
void glEnable(GLenum cap) {
  g_gl_impl->Enable(cap);
}
void glEnableVertexAttribArray(GLuint index) {
  g_gl_impl->EnableVertexAttribArray(index);
}
void glFinish() {
  g_gl_impl->Finish();
}
void glFlush() {
  g_gl_impl->Flush();
}
void glFramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint renderbuffer) {
  g_gl_impl->FramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
}
void glFramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
    GLint level) {
  g_gl_impl->FramebufferTexture2D(
      target, attachment, textarget, texture, level);
}
void glFrontFace(GLenum mode) {
  g_gl_impl->FrontFace(mode);
}
void glGenBuffers(GLsizei n, GLuint* buffers) {
  g_gl_impl->GenBuffers(n, buffers);
}
void glGenerateMipmap(GLenum target) {
  g_gl_impl->GenerateMipmap(target);
}
void glGenFramebuffers(GLsizei n, GLuint* framebuffers) {
  g_gl_impl->GenFramebuffers(n, framebuffers);
}
void glGenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  g_gl_impl->GenRenderbuffers(n, renderbuffers);
}
void glGenTextures(GLsizei n, GLuint* textures) {
  g_gl_impl->GenTextures(n, textures);
}
void glGetActiveAttrib(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  g_gl_impl->GetActiveAttrib(
      program, index, bufsize, length, size, type, name);
}
void glGetActiveUniform(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  g_gl_impl->GetActiveUniform(
      program, index, bufsize, length, size, type, name);
}
void glGetAttachedShaders(
    GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) {
  g_gl_impl->GetAttachedShaders(program, maxcount, count, shaders);
}
int glGetAttribLocation(GLuint program, const char* name) {
  return g_gl_impl->GetAttribLocation(program, name);
}
void glGetBooleanv(GLenum pname, GLboolean* params) {
  g_gl_impl->GetBooleanv(pname, params);
}
void glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  g_gl_impl->GetBufferParameteriv(target, pname, params);
}
GLenum glGetError() {
  return g_gl_impl->GetError();
}
void glGetFloatv(GLenum pname, GLfloat* params) {
  g_gl_impl->GetFloatv(pname, params);
}
void glGetFramebufferAttachmentParameteriv(
    GLenum target, GLenum attachment, GLenum pname, GLint* params) {
  g_gl_impl->GetFramebufferAttachmentParameteriv(
      target, attachment, pname, params);
}
void glGetIntegerv(GLenum pname, GLint* params) {
  g_gl_impl->GetIntegerv(pname, params);
}
void glGetProgramiv(GLuint program, GLenum pname, GLint* params) {
  g_gl_impl->GetProgramiv(program, pname, params);
}
void glGetProgramInfoLog(
    GLuint program, GLsizei bufsize, GLsizei* length, char* infolog) {
  g_gl_impl->GetProgramInfoLog(program, bufsize, length, infolog);
}
void glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  g_gl_impl->GetRenderbufferParameteriv(target, pname, params);
}
void glGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  g_gl_impl->GetShaderiv(shader, pname, params);
}
void glGetShaderInfoLog(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog) {
  g_gl_impl->GetShaderInfoLog(shader, bufsize, length, infolog);
}
void glGetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
  g_gl_impl->GetShaderPrecisionFormat(
      shadertype, precisiontype, range, precision);
}
void glGetShaderSource(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* source) {
  g_gl_impl->GetShaderSource(shader, bufsize, length, source);
}
const GLubyte* glGetString(GLenum name) {
  return g_gl_impl->GetString(name);
}
void glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
  g_gl_impl->GetTexParameterfv(target, pname, params);
}
void glGetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
  g_gl_impl->GetTexParameteriv(target, pname, params);
}
void glGetUniformfv(GLuint program, GLint location, GLfloat* params) {
  g_gl_impl->GetUniformfv(program, location, params);
}
void glGetUniformiv(GLuint program, GLint location, GLint* params) {
  g_gl_impl->GetUniformiv(program, location, params);
}
int glGetUniformLocation(GLuint program, const char* name) {
  return g_gl_impl->GetUniformLocation(program, name);
}
void glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params) {
  g_gl_impl->GetVertexAttribfv(index, pname, params);
}
void glGetVertexAttribiv(GLuint index, GLenum pname, GLint* params) {
  g_gl_impl->GetVertexAttribiv(index, pname, params);
}
void glGetVertexAttribPointerv(GLuint index, GLenum pname, void** pointer) {
  g_gl_impl->GetVertexAttribPointerv(index, pname, pointer);
}
void glHint(GLenum target, GLenum mode) {
  g_gl_impl->Hint(target, mode);
}
GLboolean glIsBuffer(GLuint buffer) {
  return g_gl_impl->IsBuffer(buffer);
}
GLboolean glIsEnabled(GLenum cap) {
  return g_gl_impl->IsEnabled(cap);
}
GLboolean glIsFramebuffer(GLuint framebuffer) {
  return g_gl_impl->IsFramebuffer(framebuffer);
}
GLboolean glIsProgram(GLuint program) {
  return g_gl_impl->IsProgram(program);
}
GLboolean glIsRenderbuffer(GLuint renderbuffer) {
  return g_gl_impl->IsRenderbuffer(renderbuffer);
}
GLboolean glIsShader(GLuint shader) {
  return g_gl_impl->IsShader(shader);
}
GLboolean glIsTexture(GLuint texture) {
  return g_gl_impl->IsTexture(texture);
}
void glLineWidth(GLfloat width) {
  g_gl_impl->LineWidth(width);
}
void glLinkProgram(GLuint program) {
  g_gl_impl->LinkProgram(program);
}
void glPixelStorei(GLenum pname, GLint param) {
  g_gl_impl->PixelStorei(pname, param);
}
void glPolygonOffset(GLfloat factor, GLfloat units) {
  g_gl_impl->PolygonOffset(factor, units);
}
void glReadPixels(
    GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
    void* pixels) {
  g_gl_impl->ReadPixels(x, y, width, height, format, type, pixels);
}
void glRenderbufferStorage(
    GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  g_gl_impl->RenderbufferStorage(target, internalformat, width, height);
}
void glSampleCoverage(GLclampf value, GLboolean invert) {
  g_gl_impl->SampleCoverage(value, invert);
}
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  g_gl_impl->Scissor(x, y, width, height);
}
void glShaderSource(
    GLuint shader, GLsizei count, const char** string, const GLint* length) {
  g_gl_impl->ShaderSource(shader, count, string, length);
}
void glStencilFunc(GLenum func, GLint ref, GLuint mask) {
  g_gl_impl->StencilFunc(func, ref, mask);
}
void glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
  g_gl_impl->StencilFuncSeparate(face, func, ref, mask);
}
void glStencilMask(GLuint mask) {
  g_gl_impl->StencilMask(mask);
}
void glStencilMaskSeparate(GLenum face, GLuint mask) {
  g_gl_impl->StencilMaskSeparate(face, mask);
}
void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  g_gl_impl->StencilOp(fail, zfail, zpass);
}
void glStencilOpSeparate(
    GLenum face, GLenum fail, GLenum zfail, GLenum zpass) {
  g_gl_impl->StencilOpSeparate(face, fail, zfail, zpass);
}
void glTexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  g_gl_impl->TexImage2D(
      target, level, internalformat, width, height, border, format, type,
      pixels);
}
void glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
  g_gl_impl->TexParameterf(target, pname, param);
}
void glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
  g_gl_impl->TexParameterfv(target, pname, params);
}
void glTexParameteri(GLenum target, GLenum pname, GLint param) {
  g_gl_impl->TexParameteri(target, pname, param);
}
void glTexParameteriv(GLenum target, GLenum pname, const GLint* params) {
  g_gl_impl->TexParameteriv(target, pname, params);
}
void glTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels) {
  g_gl_impl->TexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
}
void glUniform1f(GLint location, GLfloat x) {
  g_gl_impl->Uniform1f(location, x);
}
void glUniform1fv(GLint location, GLsizei count, const GLfloat* v) {
  g_gl_impl->Uniform1fv(location, count, v);
}
void glUniform1i(GLint location, GLint x) {
  g_gl_impl->Uniform1i(location, x);
}
void glUniform1iv(GLint location, GLsizei count, const GLint* v) {
  g_gl_impl->Uniform1iv(location, count, v);
}
void glUniform2f(GLint location, GLfloat x, GLfloat y) {
  g_gl_impl->Uniform2f(location, x, y);
}
void glUniform2fv(GLint location, GLsizei count, const GLfloat* v) {
  g_gl_impl->Uniform2fv(location, count, v);
}
void glUniform2i(GLint location, GLint x, GLint y) {
  g_gl_impl->Uniform2i(location, x, y);
}
void glUniform2iv(GLint location, GLsizei count, const GLint* v) {
  g_gl_impl->Uniform2iv(location, count, v);
}
void glUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  g_gl_impl->Uniform3f(location, x, y, z);
}
void glUniform3fv(GLint location, GLsizei count, const GLfloat* v) {
  g_gl_impl->Uniform3fv(location, count, v);
}
void glUniform3i(GLint location, GLint x, GLint y, GLint z) {
  g_gl_impl->Uniform3i(location, x, y, z);
}
void glUniform3iv(GLint location, GLsizei count, const GLint* v) {
  g_gl_impl->Uniform3iv(location, count, v);
}
void glUniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  g_gl_impl->Uniform4f(location, x, y, z, w);
}
void glUniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  g_gl_impl->Uniform4fv(location, count, v);
}
void glUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
  g_gl_impl->Uniform4i(location, x, y, z, w);
}
void glUniform4iv(GLint location, GLsizei count, const GLint* v) {
  g_gl_impl->Uniform4iv(location, count, v);
}
void glUniformMatrix2fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  g_gl_impl->UniformMatrix2fv(location, count, transpose, value);
}
void glUniformMatrix3fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  g_gl_impl->UniformMatrix3fv(location, count, transpose, value);
}
void glUniformMatrix4fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  g_gl_impl->UniformMatrix4fv(location, count, transpose, value);
}
void glUseProgram(GLuint program) {
  g_gl_impl->UseProgram(program);
}
void glValidateProgram(GLuint program) {
  g_gl_impl->ValidateProgram(program);
}
void glVertexAttrib1f(GLuint indx, GLfloat x) {
  g_gl_impl->VertexAttrib1f(indx, x);
}
void glVertexAttrib1fv(GLuint indx, const GLfloat* values) {
  g_gl_impl->VertexAttrib1fv(indx, values);
}
void glVertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
  g_gl_impl->VertexAttrib2f(indx, x, y);
}
void glVertexAttrib2fv(GLuint indx, const GLfloat* values) {
  g_gl_impl->VertexAttrib2fv(indx, values);
}
void glVertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  g_gl_impl->VertexAttrib3f(indx, x, y, z);
}
void glVertexAttrib3fv(GLuint indx, const GLfloat* values) {
  g_gl_impl->VertexAttrib3fv(indx, values);
}
void glVertexAttrib4f(
    GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  g_gl_impl->VertexAttrib4f(indx, x, y, z, w);
}
void glVertexAttrib4fv(GLuint indx, const GLfloat* values) {
  g_gl_impl->VertexAttrib4fv(indx, values);
}
void glVertexAttribPointer(
    GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void* ptr) {
  g_gl_impl->VertexAttribPointer(indx, size, type, normalized, stride, ptr);
}
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  g_gl_impl->Viewport(x, y, width, height);
}

