// This file is auto-generated. DO NOT EDIT!

// It is included by gles2_cmd_decoder.cc

parse_error::ParseError GLES2Decoder::HandleActiveTexture(
    unsigned int arg_count, const gles2::ActiveTexture& c) {
  GLenum texture = static_cast<GLenum>(c.texture);
  parse_error::ParseError result =
      ValidateActiveTexture(this, arg_count, texture);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glActiveTexture(texture);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleAttachShader(
    unsigned int arg_count, const gles2::AttachShader& c) {
  GLuint program = static_cast<GLuint>(c.program);
  GLuint shader = static_cast<GLuint>(c.shader);
  parse_error::ParseError result =
      ValidateAttachShader(this, arg_count, program, shader);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glAttachShader(program, shader);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBindAttribLocation(
    unsigned int arg_count, const gles2::BindAttribLocation& c) {
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  uint32 name_size = c.data_size;
  const char* name = GetSharedMemoryAs<const char*>(
      c.name_shm_id, c.name_shm_offset, name_size);
  parse_error::ParseError result =
      ValidateBindAttribLocation(this, arg_count, program, index, name);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  String name_str(name, name_size);
  glBindAttribLocation(program, index, name_str.c_str());
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBindAttribLocationImmediate(
    unsigned int arg_count, const gles2::BindAttribLocationImmediate& c) {
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  uint32 name_size = c.data_size;
  const char* name = GetImmediateDataAs<const char*>(c);
  // TODO(gman): Make sure validate checks arg_count
  //     covers data_size.
  parse_error::ParseError result =
      ValidateBindAttribLocationImmediate(
          this, arg_count, program, index, name);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  String name_str(name, name_size);
  glBindAttribLocation(program, index, name_str.c_str());
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBindBuffer(
    unsigned int arg_count, const gles2::BindBuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint buffer = static_cast<GLuint>(c.buffer);
  parse_error::ParseError result =
      ValidateBindBuffer(this, arg_count, target, buffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DoBindBuffer(target, buffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBindFramebuffer(
    unsigned int arg_count, const gles2::BindFramebuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint framebuffer = static_cast<GLuint>(c.framebuffer);
  parse_error::ParseError result =
      ValidateBindFramebuffer(this, arg_count, target, framebuffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBindFramebuffer(target, framebuffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBindRenderbuffer(
    unsigned int arg_count, const gles2::BindRenderbuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint renderbuffer = static_cast<GLuint>(c.renderbuffer);
  parse_error::ParseError result =
      ValidateBindRenderbuffer(this, arg_count, target, renderbuffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBindRenderbuffer(target, renderbuffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBindTexture(
    unsigned int arg_count, const gles2::BindTexture& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint texture = static_cast<GLuint>(c.texture);
  parse_error::ParseError result =
      ValidateBindTexture(this, arg_count, target, texture);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBindTexture(target, texture);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBlendColor(
    unsigned int arg_count, const gles2::BlendColor& c) {
  GLclampf red = static_cast<GLclampf>(c.red);
  GLclampf green = static_cast<GLclampf>(c.green);
  GLclampf blue = static_cast<GLclampf>(c.blue);
  GLclampf alpha = static_cast<GLclampf>(c.alpha);
  parse_error::ParseError result =
      ValidateBlendColor(this, arg_count, red, green, blue, alpha);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBlendColor(red, green, blue, alpha);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBlendEquation(
    unsigned int arg_count, const gles2::BlendEquation& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  parse_error::ParseError result =
      ValidateBlendEquation(this, arg_count, mode);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBlendEquation(mode);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBlendEquationSeparate(
    unsigned int arg_count, const gles2::BlendEquationSeparate& c) {
  GLenum modeRGB = static_cast<GLenum>(c.modeRGB);
  GLenum modeAlpha = static_cast<GLenum>(c.modeAlpha);
  parse_error::ParseError result =
      ValidateBlendEquationSeparate(this, arg_count, modeRGB, modeAlpha);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBlendEquationSeparate(modeRGB, modeAlpha);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBlendFunc(
    unsigned int arg_count, const gles2::BlendFunc& c) {
  GLenum sfactor = static_cast<GLenum>(c.sfactor);
  GLenum dfactor = static_cast<GLenum>(c.dfactor);
  parse_error::ParseError result =
      ValidateBlendFunc(this, arg_count, sfactor, dfactor);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBlendFunc(sfactor, dfactor);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBlendFuncSeparate(
    unsigned int arg_count, const gles2::BlendFuncSeparate& c) {
  GLenum srcRGB = static_cast<GLenum>(c.srcRGB);
  GLenum dstRGB = static_cast<GLenum>(c.dstRGB);
  GLenum srcAlpha = static_cast<GLenum>(c.srcAlpha);
  GLenum dstAlpha = static_cast<GLenum>(c.dstAlpha);
  parse_error::ParseError result =
      ValidateBlendFuncSeparate(
          this, arg_count, srcRGB, dstRGB, srcAlpha, dstAlpha);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBufferData(
    unsigned int arg_count, const gles2::BufferData& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32 data_shm_id = static_cast<uint32>(c.data_shm_id);
  uint32 data_shm_offset = static_cast<uint32>(c.data_shm_offset);
  GLenum usage = static_cast<GLenum>(c.usage);
  uint32 data_size = size;
  const void* data = GetSharedMemoryAs<const void*>(
      data_shm_id, data_shm_offset, data_size);
  parse_error::ParseError result =
      ValidateBufferData(this, arg_count, target, size, data, usage);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBufferData(target, size, data, usage);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBufferDataImmediate(
    unsigned int arg_count, const gles2::BufferDataImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  const void* data = GetImmediateDataAs<const void*>(c);
  GLenum usage = static_cast<GLenum>(c.usage);
  // Immediate version.
  parse_error::ParseError result =
      ValidateBufferDataImmediate(this, arg_count, target, size, data, usage);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBufferData(target, size, data, usage);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBufferSubData(
    unsigned int arg_count, const gles2::BufferSubData& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32 data_shm_id = static_cast<uint32>(c.data_shm_id);
  uint32 data_shm_offset = static_cast<uint32>(c.data_shm_offset);
  uint32 data_size = size;
  const void* data = GetSharedMemoryAs<const void*>(
      data_shm_id, data_shm_offset, data_size);
  parse_error::ParseError result =
      ValidateBufferSubData(this, arg_count, target, offset, size, data);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBufferSubData(target, offset, size, data);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleBufferSubDataImmediate(
    unsigned int arg_count, const gles2::BufferSubDataImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  const void* data = GetImmediateDataAs<const void*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateBufferSubDataImmediate(
          this, arg_count, target, offset, size, data);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBufferSubData(target, offset, size, data);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleCheckFramebufferStatus(
    unsigned int arg_count, const gles2::CheckFramebufferStatus& c) {
  GLenum target = static_cast<GLenum>(c.target);
  parse_error::ParseError result =
      ValidateCheckFramebufferStatus(this, arg_count, target);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCheckFramebufferStatus(target);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleClear(
    unsigned int arg_count, const gles2::Clear& c) {
  GLbitfield mask = static_cast<GLbitfield>(c.mask);
  parse_error::ParseError result =
      ValidateClear(this, arg_count, mask);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glClear(mask);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleClearColor(
    unsigned int arg_count, const gles2::ClearColor& c) {
  GLclampf red = static_cast<GLclampf>(c.red);
  GLclampf green = static_cast<GLclampf>(c.green);
  GLclampf blue = static_cast<GLclampf>(c.blue);
  GLclampf alpha = static_cast<GLclampf>(c.alpha);
  parse_error::ParseError result =
      ValidateClearColor(this, arg_count, red, green, blue, alpha);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glClearColor(red, green, blue, alpha);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleClearDepthf(
    unsigned int arg_count, const gles2::ClearDepthf& c) {
  GLclampf depth = static_cast<GLclampf>(c.depth);
  parse_error::ParseError result =
      ValidateClearDepthf(this, arg_count, depth);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glClearDepth(depth);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleClearStencil(
    unsigned int arg_count, const gles2::ClearStencil& c) {
  GLint s = static_cast<GLint>(c.s);
  parse_error::ParseError result =
      ValidateClearStencil(this, arg_count, s);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glClearStencil(s);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleColorMask(
    unsigned int arg_count, const gles2::ColorMask& c) {
  GLboolean red = static_cast<GLboolean>(c.red);
  GLboolean green = static_cast<GLboolean>(c.green);
  GLboolean blue = static_cast<GLboolean>(c.blue);
  GLboolean alpha = static_cast<GLboolean>(c.alpha);
  parse_error::ParseError result =
      ValidateColorMask(this, arg_count, red, green, blue, alpha);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glColorMask(red, green, blue, alpha);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleCompileShader(
    unsigned int arg_count, const gles2::CompileShader& c) {
  GLuint shader = static_cast<GLuint>(c.shader);
  parse_error::ParseError result =
      ValidateCompileShader(this, arg_count, shader);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCompileShader(shader);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleCompressedTexImage2D(
    unsigned int arg_count, const gles2::CompressedTexImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLsizei imageSize = static_cast<GLsizei>(c.imageSize);
  uint32 data_shm_id = static_cast<uint32>(c.data_shm_id);
  uint32 data_shm_offset = static_cast<uint32>(c.data_shm_offset);
  uint32 data_size = imageSize;
  const void* data = GetSharedMemoryAs<const void*>(
      data_shm_id, data_shm_offset, data_size);
  parse_error::ParseError result =
      ValidateCompressedTexImage2D(
          this, arg_count, target, level, internalformat, width, height, border,
          imageSize, data);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCompressedTexImage2D(
      target, level, internalformat, width, height, border, imageSize, data);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleCompressedTexImage2DImmediate(
    unsigned int arg_count, const gles2::CompressedTexImage2DImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLsizei imageSize = static_cast<GLsizei>(c.imageSize);
  const void* data = GetImmediateDataAs<const void*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateCompressedTexImage2DImmediate(
          this, arg_count, target, level, internalformat, width, height, border,
          imageSize, data);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCompressedTexImage2D(
      target, level, internalformat, width, height, border, imageSize, data);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleCompressedTexSubImage2D(
    unsigned int arg_count, const gles2::CompressedTexSubImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLsizei imageSize = static_cast<GLsizei>(c.imageSize);
  uint32 data_shm_id = static_cast<uint32>(c.data_shm_id);
  uint32 data_shm_offset = static_cast<uint32>(c.data_shm_offset);
  uint32 data_size = imageSize;
  const void* data = GetSharedMemoryAs<const void*>(
      data_shm_id, data_shm_offset, data_size);
  parse_error::ParseError result =
      ValidateCompressedTexSubImage2D(
          this, arg_count, target, level, xoffset, yoffset, width, height,
          format, imageSize, data);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleCompressedTexSubImage2DImmediate(
    unsigned int arg_count, const gles2::CompressedTexSubImage2DImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLsizei imageSize = static_cast<GLsizei>(c.imageSize);
  const void* data = GetImmediateDataAs<const void*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateCompressedTexSubImage2DImmediate(
          this, arg_count, target, level, xoffset, yoffset, width, height,
          format, imageSize, data);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleCopyTexImage2D(
    unsigned int arg_count, const gles2::CopyTexImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  parse_error::ParseError result =
      ValidateCopyTexImage2D(
          this, arg_count, target, level, internalformat, x, y, width, height,
          border);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleCopyTexSubImage2D(
    unsigned int arg_count, const gles2::CopyTexSubImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  parse_error::ParseError result =
      ValidateCopyTexSubImage2D(
          this, arg_count, target, level, xoffset, yoffset, x, y, width,
          height);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleCreateProgram(
    unsigned int arg_count, const gles2::CreateProgram& c) {
  uint32 client_id = c.client_id;
  parse_error::ParseError result =
      ValidateCreateProgram(this, arg_count);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  CreateProgramHelper(client_id);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleCreateShader(
    unsigned int arg_count, const gles2::CreateShader& c) {
  GLenum type = static_cast<GLenum>(c.type);
  uint32 client_id = c.client_id;
  parse_error::ParseError result =
      ValidateCreateShader(this, arg_count, type);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  CreateShaderHelper(type, client_id);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleCullFace(
    unsigned int arg_count, const gles2::CullFace& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  parse_error::ParseError result =
      ValidateCullFace(this, arg_count, mode);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCullFace(mode);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDeleteBuffers(
    unsigned int arg_count, const gles2::DeleteBuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* buffers = GetSharedMemoryAs<const GLuint*>(
      c.buffers_shm_id, c.buffers_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateDeleteBuffers(this, arg_count, n, buffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteBuffersHelper>(n, buffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDeleteBuffersImmediate(
    unsigned int arg_count, const gles2::DeleteBuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* buffers = GetImmediateDataAs<const GLuint*>(c);
  parse_error::ParseError result =
      ValidateDeleteBuffersImmediate(this, arg_count, n, buffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteBuffersHelper>(n, buffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDeleteFramebuffers(
    unsigned int arg_count, const gles2::DeleteFramebuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* framebuffers = GetSharedMemoryAs<const GLuint*>(
      c.framebuffers_shm_id, c.framebuffers_shm_offset, 0 /* TODO(
          gman): size */);
  parse_error::ParseError result =
      ValidateDeleteFramebuffers(this, arg_count, n, framebuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteFramebuffersHelper>(n, framebuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDeleteFramebuffersImmediate(
    unsigned int arg_count, const gles2::DeleteFramebuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* framebuffers = GetImmediateDataAs<const GLuint*>(c);
  parse_error::ParseError result =
      ValidateDeleteFramebuffersImmediate(this, arg_count, n, framebuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteFramebuffersHelper>(n, framebuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDeleteProgram(
    unsigned int arg_count, const gles2::DeleteProgram& c) {
  GLuint program = static_cast<GLuint>(c.program);
  parse_error::ParseError result =
      ValidateDeleteProgram(this, arg_count, program);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DoDeleteProgram(program);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDeleteRenderbuffers(
    unsigned int arg_count, const gles2::DeleteRenderbuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* renderbuffers = GetSharedMemoryAs<const GLuint*>(
      c.renderbuffers_shm_id, c.renderbuffers_shm_offset, 0 /* TODO(
          gman): size */);
  parse_error::ParseError result =
      ValidateDeleteRenderbuffers(this, arg_count, n, renderbuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteRenderbuffersHelper>(n, renderbuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDeleteRenderbuffersImmediate(
    unsigned int arg_count, const gles2::DeleteRenderbuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* renderbuffers = GetImmediateDataAs<const GLuint*>(c);
  parse_error::ParseError result =
      ValidateDeleteRenderbuffersImmediate(this, arg_count, n, renderbuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteRenderbuffersHelper>(n, renderbuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDeleteShader(
    unsigned int arg_count, const gles2::DeleteShader& c) {
  GLuint shader = static_cast<GLuint>(c.shader);
  parse_error::ParseError result =
      ValidateDeleteShader(this, arg_count, shader);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DoDeleteShader(shader);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDeleteTextures(
    unsigned int arg_count, const gles2::DeleteTextures& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* textures = GetSharedMemoryAs<const GLuint*>(
      c.textures_shm_id, c.textures_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateDeleteTextures(this, arg_count, n, textures);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteTexturesHelper>(n, textures);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDeleteTexturesImmediate(
    unsigned int arg_count, const gles2::DeleteTexturesImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* textures = GetImmediateDataAs<const GLuint*>(c);
  parse_error::ParseError result =
      ValidateDeleteTexturesImmediate(this, arg_count, n, textures);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteTexturesHelper>(n, textures);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDepthFunc(
    unsigned int arg_count, const gles2::DepthFunc& c) {
  GLenum func = static_cast<GLenum>(c.func);
  parse_error::ParseError result =
      ValidateDepthFunc(this, arg_count, func);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDepthFunc(func);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDepthMask(
    unsigned int arg_count, const gles2::DepthMask& c) {
  GLboolean flag = static_cast<GLboolean>(c.flag);
  parse_error::ParseError result =
      ValidateDepthMask(this, arg_count, flag);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDepthMask(flag);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDepthRangef(
    unsigned int arg_count, const gles2::DepthRangef& c) {
  GLclampf zNear = static_cast<GLclampf>(c.zNear);
  GLclampf zFar = static_cast<GLclampf>(c.zFar);
  parse_error::ParseError result =
      ValidateDepthRangef(this, arg_count, zNear, zFar);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDepthRange(zNear, zFar);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDetachShader(
    unsigned int arg_count, const gles2::DetachShader& c) {
  GLuint program = static_cast<GLuint>(c.program);
  GLuint shader = static_cast<GLuint>(c.shader);
  parse_error::ParseError result =
      ValidateDetachShader(this, arg_count, program, shader);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDetachShader(program, shader);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDisable(
    unsigned int arg_count, const gles2::Disable& c) {
  GLenum cap = static_cast<GLenum>(c.cap);
  parse_error::ParseError result =
      ValidateDisable(this, arg_count, cap);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDisable(cap);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDisableVertexAttribArray(
    unsigned int arg_count, const gles2::DisableVertexAttribArray& c) {
  GLuint index = static_cast<GLuint>(c.index);
  parse_error::ParseError result =
      ValidateDisableVertexAttribArray(this, arg_count, index);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDisableVertexAttribArray(index);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleDrawArrays(
    unsigned int arg_count, const gles2::DrawArrays& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  GLint first = static_cast<GLint>(c.first);
  GLsizei count = static_cast<GLsizei>(c.count);
  parse_error::ParseError result =
      ValidateDrawArrays(this, arg_count, mode, first, count);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDrawArrays(mode, first, count);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleEnable(
    unsigned int arg_count, const gles2::Enable& c) {
  GLenum cap = static_cast<GLenum>(c.cap);
  parse_error::ParseError result =
      ValidateEnable(this, arg_count, cap);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glEnable(cap);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleEnableVertexAttribArray(
    unsigned int arg_count, const gles2::EnableVertexAttribArray& c) {
  GLuint index = static_cast<GLuint>(c.index);
  parse_error::ParseError result =
      ValidateEnableVertexAttribArray(this, arg_count, index);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glEnableVertexAttribArray(index);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleFinish(
    unsigned int arg_count, const gles2::Finish& c) {
  parse_error::ParseError result =
      ValidateFinish(this, arg_count);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glFinish();
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleFlush(
    unsigned int arg_count, const gles2::Flush& c) {
  parse_error::ParseError result =
      ValidateFlush(this, arg_count);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glFlush();
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleFramebufferRenderbuffer(
    unsigned int arg_count, const gles2::FramebufferRenderbuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum renderbuffertarget = static_cast<GLenum>(c.renderbuffertarget);
  GLuint renderbuffer = static_cast<GLuint>(c.renderbuffer);
  parse_error::ParseError result =
      ValidateFramebufferRenderbuffer(
          this, arg_count, target, attachment, renderbuffertarget,
          renderbuffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glFramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleFramebufferTexture2D(
    unsigned int arg_count, const gles2::FramebufferTexture2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum textarget = static_cast<GLenum>(c.textarget);
  GLuint texture = static_cast<GLuint>(c.texture);
  GLint level = static_cast<GLint>(c.level);
  parse_error::ParseError result =
      ValidateFramebufferTexture2D(
          this, arg_count, target, attachment, textarget, texture, level);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glFramebufferTexture2D(target, attachment, textarget, texture, level);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleFrontFace(
    unsigned int arg_count, const gles2::FrontFace& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  parse_error::ParseError result =
      ValidateFrontFace(this, arg_count, mode);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glFrontFace(mode);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGenBuffers(
    unsigned int arg_count, const gles2::GenBuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* buffers = GetSharedMemoryAs<GLuint*>(
      c.buffers_shm_id, c.buffers_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateGenBuffers(this, arg_count, n, buffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenBuffersHelper>(n, buffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGenBuffersImmediate(
    unsigned int arg_count, const gles2::GenBuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* buffers = GetImmediateDataAs<GLuint*>(c);
  parse_error::ParseError result =
      ValidateGenBuffersImmediate(this, arg_count, n, buffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenBuffersHelper>(n, buffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGenerateMipmap(
    unsigned int arg_count, const gles2::GenerateMipmap& c) {
  GLenum target = static_cast<GLenum>(c.target);
  parse_error::ParseError result =
      ValidateGenerateMipmap(this, arg_count, target);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGenerateMipmap(target);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGenFramebuffers(
    unsigned int arg_count, const gles2::GenFramebuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* framebuffers = GetSharedMemoryAs<GLuint*>(
      c.framebuffers_shm_id, c.framebuffers_shm_offset, 0 /* TODO(
          gman): size */);
  parse_error::ParseError result =
      ValidateGenFramebuffers(this, arg_count, n, framebuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenFramebuffersHelper>(n, framebuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGenFramebuffersImmediate(
    unsigned int arg_count, const gles2::GenFramebuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* framebuffers = GetImmediateDataAs<GLuint*>(c);
  parse_error::ParseError result =
      ValidateGenFramebuffersImmediate(this, arg_count, n, framebuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenFramebuffersHelper>(n, framebuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGenRenderbuffers(
    unsigned int arg_count, const gles2::GenRenderbuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* renderbuffers = GetSharedMemoryAs<GLuint*>(
      c.renderbuffers_shm_id, c.renderbuffers_shm_offset, 0 /* TODO(
          gman): size */);
  parse_error::ParseError result =
      ValidateGenRenderbuffers(this, arg_count, n, renderbuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenRenderbuffersHelper>(n, renderbuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGenRenderbuffersImmediate(
    unsigned int arg_count, const gles2::GenRenderbuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* renderbuffers = GetImmediateDataAs<GLuint*>(c);
  parse_error::ParseError result =
      ValidateGenRenderbuffersImmediate(this, arg_count, n, renderbuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenRenderbuffersHelper>(n, renderbuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGenTextures(
    unsigned int arg_count, const gles2::GenTextures& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* textures = GetSharedMemoryAs<GLuint*>(
      c.textures_shm_id, c.textures_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateGenTextures(this, arg_count, n, textures);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenTexturesHelper>(n, textures);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGenTexturesImmediate(
    unsigned int arg_count, const gles2::GenTexturesImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* textures = GetImmediateDataAs<GLuint*>(c);
  parse_error::ParseError result =
      ValidateGenTexturesImmediate(this, arg_count, n, textures);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenTexturesHelper>(n, textures);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetAttribLocation(
    unsigned int arg_count, const gles2::GetAttribLocation& c) {
  GLuint program = static_cast<GLuint>(c.program);
  uint32 name_size = c.data_size;
  const char* name = GetSharedMemoryAs<const char*>(
      c.name_shm_id, c.name_shm_offset, name_size);
  parse_error::ParseError result =
      ValidateGetAttribLocation(this, arg_count, program, name);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  String name_str(name, name_size);
  glGetAttribLocation(program, name_str.c_str());
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetAttribLocationImmediate(
    unsigned int arg_count, const gles2::GetAttribLocationImmediate& c) {
  GLuint program = static_cast<GLuint>(c.program);
  uint32 name_size = c.data_size;
  const char* name = GetImmediateDataAs<const char*>(c);
  // TODO(gman): Make sure validate checks arg_count
  //     covers data_size.
  parse_error::ParseError result =
      ValidateGetAttribLocationImmediate(this, arg_count, program, name);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  String name_str(name, name_size);
  glGetAttribLocation(program, name_str.c_str());
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetBooleanv(
    unsigned int arg_count, const gles2::GetBooleanv& c) {
  GLenum pname = static_cast<GLenum>(c.pname);
  GLboolean* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLboolean*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetBooleanv(this, arg_count, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetBooleanv(pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetBufferParameteriv(
    unsigned int arg_count, const gles2::GetBufferParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetBufferParameteriv(this, arg_count, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetBufferParameteriv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetError(
    unsigned int arg_count, const gles2::GetError& c) {
  GLenum* result_dst = GetSharedMemoryAs<GLenum*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateGetError(this, arg_count);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glGetError();
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetFloatv(
    unsigned int arg_count, const gles2::GetFloatv& c) {
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLfloat*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetFloatv(this, arg_count, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetFloatv(pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetFramebufferAttachmentParameteriv(
    
    unsigned int arg_count,
    const gles2::GetFramebufferAttachmentParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetFramebufferAttachmentParameteriv(
          this, arg_count, target, attachment, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetIntegerv(
    unsigned int arg_count, const gles2::GetIntegerv& c) {
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetIntegerv(this, arg_count, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetIntegerv(pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetProgramiv(
    unsigned int arg_count, const gles2::GetProgramiv& c) {
  GLuint program = static_cast<GLuint>(c.program);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetProgramiv(this, arg_count, program, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetProgramiv(program, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetProgramInfoLog(
    unsigned int arg_count, const gles2::GetProgramInfoLog& c) {
  GLuint program = static_cast<GLuint>(c.program);
  GLsizei bufsize = static_cast<GLsizei>(c.bufsize);
  GLsizei* length = GetSharedMemoryAs<GLsizei*>(
      c.length_shm_id, c.length_shm_offset, 0 /* TODO(gman): size */);
  char* infolog = GetSharedMemoryAs<char*>(
      c.infolog_shm_id, c.infolog_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateGetProgramInfoLog(
          this, arg_count, program, bufsize, length, infolog);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetProgramInfoLog(program, bufsize, length, infolog);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetRenderbufferParameteriv(
    unsigned int arg_count, const gles2::GetRenderbufferParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetRenderbufferParameteriv(
          this, arg_count, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetRenderbufferParameteriv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetShaderiv(
    unsigned int arg_count, const gles2::GetShaderiv& c) {
  GLuint shader = static_cast<GLuint>(c.shader);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetShaderiv(this, arg_count, shader, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetShaderiv(shader, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetShaderInfoLog(
    unsigned int arg_count, const gles2::GetShaderInfoLog& c) {
  GLuint shader = static_cast<GLuint>(c.shader);
  GLsizei bufsize = static_cast<GLsizei>(c.bufsize);
  GLsizei* length = GetSharedMemoryAs<GLsizei*>(
      c.length_shm_id, c.length_shm_offset, 0 /* TODO(gman): size */);
  char* infolog = GetSharedMemoryAs<char*>(
      c.infolog_shm_id, c.infolog_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateGetShaderInfoLog(
          this, arg_count, shader, bufsize, length, infolog);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetShaderInfoLog(shader, bufsize, length, infolog);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetShaderSource(
    unsigned int arg_count, const gles2::GetShaderSource& c) {
  GLuint shader = static_cast<GLuint>(c.shader);
  GLsizei bufsize = static_cast<GLsizei>(c.bufsize);
  GLsizei* length = GetSharedMemoryAs<GLsizei*>(
      c.length_shm_id, c.length_shm_offset, 0 /* TODO(gman): size */);
  char* source = GetSharedMemoryAs<char*>(
      c.source_shm_id, c.source_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateGetShaderSource(
          this, arg_count, shader, bufsize, length, source);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetShaderSource(shader, bufsize, length, source);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetString(
    unsigned int arg_count, const gles2::GetString& c) {
  GLenum name = static_cast<GLenum>(c.name);
  parse_error::ParseError result =
      ValidateGetString(this, arg_count, name);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetString(name);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetTexParameterfv(
    unsigned int arg_count, const gles2::GetTexParameterfv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLfloat*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetTexParameterfv(this, arg_count, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetTexParameterfv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetTexParameteriv(
    unsigned int arg_count, const gles2::GetTexParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetTexParameteriv(this, arg_count, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetTexParameteriv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetUniformLocation(
    unsigned int arg_count, const gles2::GetUniformLocation& c) {
  GLuint program = static_cast<GLuint>(c.program);
  uint32 name_size = c.data_size;
  const char* name = GetSharedMemoryAs<const char*>(
      c.name_shm_id, c.name_shm_offset, name_size);
  parse_error::ParseError result =
      ValidateGetUniformLocation(this, arg_count, program, name);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  String name_str(name, name_size);
  glGetUniformLocation(program, name_str.c_str());
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetUniformLocationImmediate(
    unsigned int arg_count, const gles2::GetUniformLocationImmediate& c) {
  GLuint program = static_cast<GLuint>(c.program);
  uint32 name_size = c.data_size;
  const char* name = GetImmediateDataAs<const char*>(c);
  // TODO(gman): Make sure validate checks arg_count
  //     covers data_size.
  parse_error::ParseError result =
      ValidateGetUniformLocationImmediate(this, arg_count, program, name);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  String name_str(name, name_size);
  glGetUniformLocation(program, name_str.c_str());
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetVertexAttribfv(
    unsigned int arg_count, const gles2::GetVertexAttribfv& c) {
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLfloat*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetVertexAttribfv(this, arg_count, index, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetVertexAttribfv(index, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleGetVertexAttribiv(
    unsigned int arg_count, const gles2::GetVertexAttribiv& c) {
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetVertexAttribiv(this, arg_count, index, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetVertexAttribiv(index, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleHint(
    unsigned int arg_count, const gles2::Hint& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum mode = static_cast<GLenum>(c.mode);
  parse_error::ParseError result =
      ValidateHint(this, arg_count, target, mode);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glHint(target, mode);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleIsBuffer(
    unsigned int arg_count, const gles2::IsBuffer& c) {
  GLuint buffer = static_cast<GLuint>(c.buffer);
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsBuffer(this, arg_count, buffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsBuffer(buffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleIsEnabled(
    unsigned int arg_count, const gles2::IsEnabled& c) {
  GLenum cap = static_cast<GLenum>(c.cap);
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsEnabled(this, arg_count, cap);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsEnabled(cap);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleIsFramebuffer(
    unsigned int arg_count, const gles2::IsFramebuffer& c) {
  GLuint framebuffer = static_cast<GLuint>(c.framebuffer);
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsFramebuffer(this, arg_count, framebuffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsFramebuffer(framebuffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleIsProgram(
    unsigned int arg_count, const gles2::IsProgram& c) {
  GLuint program = static_cast<GLuint>(c.program);
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsProgram(this, arg_count, program);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsProgram(program);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleIsRenderbuffer(
    unsigned int arg_count, const gles2::IsRenderbuffer& c) {
  GLuint renderbuffer = static_cast<GLuint>(c.renderbuffer);
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsRenderbuffer(this, arg_count, renderbuffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsRenderbuffer(renderbuffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleIsShader(
    unsigned int arg_count, const gles2::IsShader& c) {
  GLuint shader = static_cast<GLuint>(c.shader);
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsShader(this, arg_count, shader);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsShader(shader);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleIsTexture(
    unsigned int arg_count, const gles2::IsTexture& c) {
  GLuint texture = static_cast<GLuint>(c.texture);
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsTexture(this, arg_count, texture);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsTexture(texture);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleLineWidth(
    unsigned int arg_count, const gles2::LineWidth& c) {
  GLfloat width = static_cast<GLfloat>(c.width);
  parse_error::ParseError result =
      ValidateLineWidth(this, arg_count, width);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glLineWidth(width);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleLinkProgram(
    unsigned int arg_count, const gles2::LinkProgram& c) {
  GLuint program = static_cast<GLuint>(c.program);
  parse_error::ParseError result =
      ValidateLinkProgram(this, arg_count, program);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glLinkProgram(program);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandlePolygonOffset(
    unsigned int arg_count, const gles2::PolygonOffset& c) {
  GLfloat factor = static_cast<GLfloat>(c.factor);
  GLfloat units = static_cast<GLfloat>(c.units);
  parse_error::ParseError result =
      ValidatePolygonOffset(this, arg_count, factor, units);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glPolygonOffset(factor, units);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleRenderbufferStorage(
    unsigned int arg_count, const gles2::RenderbufferStorage& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  parse_error::ParseError result =
      ValidateRenderbufferStorage(
          this, arg_count, target, internalformat, width, height);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glRenderbufferStorage(target, internalformat, width, height);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleSampleCoverage(
    unsigned int arg_count, const gles2::SampleCoverage& c) {
  GLclampf value = static_cast<GLclampf>(c.value);
  GLboolean invert = static_cast<GLboolean>(c.invert);
  parse_error::ParseError result =
      ValidateSampleCoverage(this, arg_count, value, invert);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glSampleCoverage(value, invert);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleScissor(
    unsigned int arg_count, const gles2::Scissor& c) {
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  parse_error::ParseError result =
      ValidateScissor(this, arg_count, x, y, width, height);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glScissor(x, y, width, height);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleStencilFunc(
    unsigned int arg_count, const gles2::StencilFunc& c) {
  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  parse_error::ParseError result =
      ValidateStencilFunc(this, arg_count, func, ref, mask);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glStencilFunc(func, ref, mask);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleStencilFuncSeparate(
    unsigned int arg_count, const gles2::StencilFuncSeparate& c) {
  GLenum face = static_cast<GLenum>(c.face);
  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  parse_error::ParseError result =
      ValidateStencilFuncSeparate(this, arg_count, face, func, ref, mask);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glStencilFuncSeparate(face, func, ref, mask);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleStencilMask(
    unsigned int arg_count, const gles2::StencilMask& c) {
  GLuint mask = static_cast<GLuint>(c.mask);
  parse_error::ParseError result =
      ValidateStencilMask(this, arg_count, mask);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glStencilMask(mask);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleStencilMaskSeparate(
    unsigned int arg_count, const gles2::StencilMaskSeparate& c) {
  GLenum face = static_cast<GLenum>(c.face);
  GLuint mask = static_cast<GLuint>(c.mask);
  parse_error::ParseError result =
      ValidateStencilMaskSeparate(this, arg_count, face, mask);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glStencilMaskSeparate(face, mask);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleStencilOp(
    unsigned int arg_count, const gles2::StencilOp& c) {
  GLenum fail = static_cast<GLenum>(c.fail);
  GLenum zfail = static_cast<GLenum>(c.zfail);
  GLenum zpass = static_cast<GLenum>(c.zpass);
  parse_error::ParseError result =
      ValidateStencilOp(this, arg_count, fail, zfail, zpass);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glStencilOp(fail, zfail, zpass);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleStencilOpSeparate(
    unsigned int arg_count, const gles2::StencilOpSeparate& c) {
  GLenum face = static_cast<GLenum>(c.face);
  GLenum fail = static_cast<GLenum>(c.fail);
  GLenum zfail = static_cast<GLenum>(c.zfail);
  GLenum zpass = static_cast<GLenum>(c.zpass);
  parse_error::ParseError result =
      ValidateStencilOpSeparate(this, arg_count, face, fail, zfail, zpass);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glStencilOpSeparate(face, fail, zfail, zpass);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleTexImage2D(
    unsigned int arg_count, const gles2::TexImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint internalformat = static_cast<GLint>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32 pixels_shm_id = static_cast<uint32>(c.pixels_shm_id);
  uint32 pixels_shm_offset = static_cast<uint32>(c.pixels_shm_offset);
  uint32 pixels_size = GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_);
  const void* pixels = GetSharedMemoryAs<const void*>(
      pixels_shm_id, pixels_shm_offset, pixels_size);
  parse_error::ParseError result =
      ValidateTexImage2D(
          this, arg_count, target, level, internalformat, width, height, border,
          format, type, pixels);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexImage2D(
      target, level, internalformat, width, height, border, format, type,
      pixels);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleTexImage2DImmediate(
    unsigned int arg_count, const gles2::TexImage2DImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint internalformat = static_cast<GLint>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  const void* pixels = GetImmediateDataAs<const void*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateTexImage2DImmediate(
          this, arg_count, target, level, internalformat, width, height, border,
          format, type, pixels);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexImage2D(
      target, level, internalformat, width, height, border, format, type,
      pixels);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleTexParameterf(
    unsigned int arg_count, const gles2::TexParameterf& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat param = static_cast<GLfloat>(c.param);
  parse_error::ParseError result =
      ValidateTexParameterf(this, arg_count, target, pname, param);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexParameterf(target, pname, param);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleTexParameterfv(
    unsigned int arg_count, const gles2::TexParameterfv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  const GLfloat* params = GetSharedMemoryAs<const GLfloat*>(
      c.params_shm_id, c.params_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateTexParameterfv(this, arg_count, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexParameterfv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleTexParameterfvImmediate(
    unsigned int arg_count, const gles2::TexParameterfvImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  const GLfloat* params = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateTexParameterfvImmediate(this, arg_count, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexParameterfv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleTexParameteri(
    unsigned int arg_count, const gles2::TexParameteri& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint param = static_cast<GLint>(c.param);
  parse_error::ParseError result =
      ValidateTexParameteri(this, arg_count, target, pname, param);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexParameteri(target, pname, param);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleTexParameteriv(
    unsigned int arg_count, const gles2::TexParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  const GLint* params = GetSharedMemoryAs<const GLint*>(
      c.params_shm_id, c.params_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateTexParameteriv(this, arg_count, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexParameteriv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleTexParameterivImmediate(
    unsigned int arg_count, const gles2::TexParameterivImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  const GLint* params = GetImmediateDataAs<const GLint*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateTexParameterivImmediate(this, arg_count, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexParameteriv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleTexSubImage2D(
    unsigned int arg_count, const gles2::TexSubImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32 pixels_shm_id = static_cast<uint32>(c.pixels_shm_id);
  uint32 pixels_shm_offset = static_cast<uint32>(c.pixels_shm_offset);
  uint32 pixels_size = GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_);
  const void* pixels = GetSharedMemoryAs<const void*>(
      pixels_shm_id, pixels_shm_offset, pixels_size);
  parse_error::ParseError result =
      ValidateTexSubImage2D(
          this, arg_count, target, level, xoffset, yoffset, width, height,
          format, type, pixels);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleTexSubImage2DImmediate(
    unsigned int arg_count, const gles2::TexSubImage2DImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  const void* pixels = GetImmediateDataAs<const void*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateTexSubImage2DImmediate(
          this, arg_count, target, level, xoffset, yoffset, width, height,
          format, type, pixels);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform1f(
    unsigned int arg_count, const gles2::Uniform1f& c) {
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  parse_error::ParseError result =
      ValidateUniform1f(this, arg_count, location, x);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform1f(location, x);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform1fv(
    unsigned int arg_count, const gles2::Uniform1fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetSharedMemoryAs<const GLfloat*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform1fv(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform1fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform1fvImmediate(
    unsigned int arg_count, const gles2::Uniform1fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform1fvImmediate(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform1fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform1i(
    unsigned int arg_count, const gles2::Uniform1i& c) {
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  parse_error::ParseError result =
      ValidateUniform1i(this, arg_count, location, x);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform1i(location, x);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform1iv(
    unsigned int arg_count, const gles2::Uniform1iv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetSharedMemoryAs<const GLint*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform1iv(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform1iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform1ivImmediate(
    unsigned int arg_count, const gles2::Uniform1ivImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetImmediateDataAs<const GLint*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform1ivImmediate(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform1iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform2f(
    unsigned int arg_count, const gles2::Uniform2f& c) {
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  parse_error::ParseError result =
      ValidateUniform2f(this, arg_count, location, x, y);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform2f(location, x, y);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform2fv(
    unsigned int arg_count, const gles2::Uniform2fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetSharedMemoryAs<const GLfloat*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform2fv(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform2fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform2fvImmediate(
    unsigned int arg_count, const gles2::Uniform2fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform2fvImmediate(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform2fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform2i(
    unsigned int arg_count, const gles2::Uniform2i& c) {
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  parse_error::ParseError result =
      ValidateUniform2i(this, arg_count, location, x, y);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform2i(location, x, y);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform2iv(
    unsigned int arg_count, const gles2::Uniform2iv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetSharedMemoryAs<const GLint*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform2iv(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform2iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform2ivImmediate(
    unsigned int arg_count, const gles2::Uniform2ivImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetImmediateDataAs<const GLint*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform2ivImmediate(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform2iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform3f(
    unsigned int arg_count, const gles2::Uniform3f& c) {
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  parse_error::ParseError result =
      ValidateUniform3f(this, arg_count, location, x, y, z);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform3f(location, x, y, z);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform3fv(
    unsigned int arg_count, const gles2::Uniform3fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetSharedMemoryAs<const GLfloat*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform3fv(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform3fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform3fvImmediate(
    unsigned int arg_count, const gles2::Uniform3fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform3fvImmediate(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform3fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform3i(
    unsigned int arg_count, const gles2::Uniform3i& c) {
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  parse_error::ParseError result =
      ValidateUniform3i(this, arg_count, location, x, y, z);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform3i(location, x, y, z);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform3iv(
    unsigned int arg_count, const gles2::Uniform3iv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetSharedMemoryAs<const GLint*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform3iv(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform3iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform3ivImmediate(
    unsigned int arg_count, const gles2::Uniform3ivImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetImmediateDataAs<const GLint*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform3ivImmediate(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform3iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform4f(
    unsigned int arg_count, const gles2::Uniform4f& c) {
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat w = static_cast<GLfloat>(c.w);
  parse_error::ParseError result =
      ValidateUniform4f(this, arg_count, location, x, y, z, w);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform4f(location, x, y, z, w);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform4fv(
    unsigned int arg_count, const gles2::Uniform4fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetSharedMemoryAs<const GLfloat*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform4fv(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform4fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform4fvImmediate(
    unsigned int arg_count, const gles2::Uniform4fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform4fvImmediate(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform4fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform4i(
    unsigned int arg_count, const gles2::Uniform4i& c) {
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  GLint w = static_cast<GLint>(c.w);
  parse_error::ParseError result =
      ValidateUniform4i(this, arg_count, location, x, y, z, w);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform4i(location, x, y, z, w);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform4iv(
    unsigned int arg_count, const gles2::Uniform4iv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetSharedMemoryAs<const GLint*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform4iv(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform4iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniform4ivImmediate(
    unsigned int arg_count, const gles2::Uniform4ivImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetImmediateDataAs<const GLint*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform4ivImmediate(this, arg_count, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform4iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniformMatrix2fv(
    unsigned int arg_count, const gles2::UniformMatrix2fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  const GLfloat* value = GetSharedMemoryAs<const GLfloat*>(
      c.value_shm_id, c.value_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniformMatrix2fv(
          this, arg_count, location, count, transpose, value);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniformMatrix2fv(location, count, transpose, value);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniformMatrix2fvImmediate(
    unsigned int arg_count, const gles2::UniformMatrix2fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  const GLfloat* value = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniformMatrix2fvImmediate(
          this, arg_count, location, count, transpose, value);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniformMatrix2fv(location, count, transpose, value);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniformMatrix3fv(
    unsigned int arg_count, const gles2::UniformMatrix3fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  const GLfloat* value = GetSharedMemoryAs<const GLfloat*>(
      c.value_shm_id, c.value_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniformMatrix3fv(
          this, arg_count, location, count, transpose, value);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniformMatrix3fv(location, count, transpose, value);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniformMatrix3fvImmediate(
    unsigned int arg_count, const gles2::UniformMatrix3fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  const GLfloat* value = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniformMatrix3fvImmediate(
          this, arg_count, location, count, transpose, value);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniformMatrix3fv(location, count, transpose, value);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniformMatrix4fv(
    unsigned int arg_count, const gles2::UniformMatrix4fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  const GLfloat* value = GetSharedMemoryAs<const GLfloat*>(
      c.value_shm_id, c.value_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniformMatrix4fv(
          this, arg_count, location, count, transpose, value);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniformMatrix4fv(location, count, transpose, value);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUniformMatrix4fvImmediate(
    unsigned int arg_count, const gles2::UniformMatrix4fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  const GLfloat* value = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniformMatrix4fvImmediate(
          this, arg_count, location, count, transpose, value);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniformMatrix4fv(location, count, transpose, value);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleUseProgram(
    unsigned int arg_count, const gles2::UseProgram& c) {
  GLuint program = static_cast<GLuint>(c.program);
  parse_error::ParseError result =
      ValidateUseProgram(this, arg_count, program);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUseProgram(program);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleValidateProgram(
    unsigned int arg_count, const gles2::ValidateProgram& c) {
  GLuint program = static_cast<GLuint>(c.program);
  parse_error::ParseError result =
      ValidateValidateProgram(this, arg_count, program);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glValidateProgram(program);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleVertexAttrib1f(
    unsigned int arg_count, const gles2::VertexAttrib1f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  parse_error::ParseError result =
      ValidateVertexAttrib1f(this, arg_count, indx, x);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib1f(indx, x);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleVertexAttrib1fv(
    unsigned int arg_count, const gles2::VertexAttrib1fv& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetSharedMemoryAs<const GLfloat*>(
      c.values_shm_id, c.values_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateVertexAttrib1fv(this, arg_count, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib1fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleVertexAttrib1fvImmediate(
    unsigned int arg_count, const gles2::VertexAttrib1fvImmediate& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateVertexAttrib1fvImmediate(this, arg_count, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib1fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleVertexAttrib2f(
    unsigned int arg_count, const gles2::VertexAttrib2f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  parse_error::ParseError result =
      ValidateVertexAttrib2f(this, arg_count, indx, x, y);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib2f(indx, x, y);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleVertexAttrib2fv(
    unsigned int arg_count, const gles2::VertexAttrib2fv& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetSharedMemoryAs<const GLfloat*>(
      c.values_shm_id, c.values_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateVertexAttrib2fv(this, arg_count, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib2fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleVertexAttrib2fvImmediate(
    unsigned int arg_count, const gles2::VertexAttrib2fvImmediate& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateVertexAttrib2fvImmediate(this, arg_count, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib2fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleVertexAttrib3f(
    unsigned int arg_count, const gles2::VertexAttrib3f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  parse_error::ParseError result =
      ValidateVertexAttrib3f(this, arg_count, indx, x, y, z);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib3f(indx, x, y, z);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleVertexAttrib3fv(
    unsigned int arg_count, const gles2::VertexAttrib3fv& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetSharedMemoryAs<const GLfloat*>(
      c.values_shm_id, c.values_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateVertexAttrib3fv(this, arg_count, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib3fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleVertexAttrib3fvImmediate(
    unsigned int arg_count, const gles2::VertexAttrib3fvImmediate& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateVertexAttrib3fvImmediate(this, arg_count, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib3fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleVertexAttrib4f(
    unsigned int arg_count, const gles2::VertexAttrib4f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat w = static_cast<GLfloat>(c.w);
  parse_error::ParseError result =
      ValidateVertexAttrib4f(this, arg_count, indx, x, y, z, w);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib4f(indx, x, y, z, w);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleVertexAttrib4fv(
    unsigned int arg_count, const gles2::VertexAttrib4fv& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetSharedMemoryAs<const GLfloat*>(
      c.values_shm_id, c.values_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateVertexAttrib4fv(this, arg_count, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib4fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleVertexAttrib4fvImmediate(
    unsigned int arg_count, const gles2::VertexAttrib4fvImmediate& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateVertexAttrib4fvImmediate(this, arg_count, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib4fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2Decoder::HandleViewport(
    unsigned int arg_count, const gles2::Viewport& c) {
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  parse_error::ParseError result =
      ValidateViewport(this, arg_count, x, y, width, height);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glViewport(x, y, width, height);
  return parse_error::kParseNoError;
}

