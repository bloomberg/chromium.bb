// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class to emluate GLES2 over command buffers.

#include "gpu/command_buffer/client/gles2_implementation.h"
// TODO(gman): remove when all functions have been implemented.
#include "gpu/command_buffer/client/gles2_implementation_gen.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"

namespace command_buffer {
namespace gles2 {

GLES2Implementation::GLES2Implementation(
      GLES2CmdHelper* helper,
      ResourceId shared_memory_id,
      void* shared_memory_address)
    : util_(0),  // TODO(gman): Get real number of compressed texture formats.
      helper_(helper),
      shared_memory_(shared_memory_id, shared_memory_address),
      pack_alignment_(4),
      unpack_alignment_(4) {
}

void GLES2Implementation::MakeIds(GLsizei n, GLuint* ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    ids[ii] = id_allocator_.AllocateID();
  }
}

void GLES2Implementation::FreeIds(GLsizei n, const GLuint* ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    id_allocator_.FreeID(ids[ii]);
  }
}

void GLES2Implementation::ShaderSource(
    GLuint shader, GLsizei count, const char** string, const GLint* length) {
  // TODO(gman): change to use buckets and check that there is enough room.
  uint32* offsets = shared_memory_.GetAddressAs<uint32*>(0);
  char* strings = reinterpret_cast<char*>(offsets + count);

  uint32 offset = count * sizeof(*offsets);
  for (GLsizei ii = 0; ii < count; ++ii) {
    uint32 len = length ? length[ii] : strlen(string[ii]);
    memcpy(strings + offset, string[ii], len);
    offset += len;
    offsets[ii] = offset;
  }

  helper_->ShaderSource(shader, count, shared_memory_.GetId(), 0, offset);
  // TODO(gman): Should insert token but not wait until we need shared memory
  //     again. Really, I should implement a shared memory manager that puts
  //     things in the next unused part of shared memory and only blocks
  //     when it needs more memory.
  int32 token = helper_->InsertToken();
  helper_->WaitForToken(token);
}

void GLES2Implementation::BufferData(
    GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
  // TODO(gman): Switch to use buckets alwayst or at least if no room in shared
  //    memory.
  memcpy(shared_memory_.GetAddress(0), data, size);
  helper_->BufferData(target, size, shared_memory_.GetId(), 0, usage);
  int32 token = helper_->InsertToken();
  helper_->WaitForToken(token);
}

void GLES2Implementation::BufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
  // TODO(gman): Switch to use buckets alwayst or at least if no room in shared
  //    memory.
  memcpy(shared_memory_.GetAddress(0), data, size);
  helper_->BufferSubData(target, offset, size, shared_memory_.GetId(), 0);
  int32 token = helper_->InsertToken();
  helper_->WaitForToken(token);
}

void GLES2Implementation::CompressedTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLsizei width,
    GLsizei height, GLint border, GLsizei imageSize, const void* data) {
  // TODO(gman): Switch to use buckets alwayst or at least if no room in shared
  //    memory.
  memcpy(shared_memory_.GetAddress(0), data, imageSize);
  helper_->CompressedTexImage2D(
      target, level, internalformat, width, height, border, imageSize,
      shared_memory_.GetId(), 0);
  int32 token = helper_->InsertToken();
  helper_->WaitForToken(token);
}

void GLES2Implementation::CompressedTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
  // TODO(gman): Switch to use buckets alwayst or at least if no room in shared
  //    memory.
  memcpy(shared_memory_.GetAddress(0), data, imageSize);
  helper_->CompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize,
      shared_memory_.GetId(), 0);
  int32 token = helper_->InsertToken();
  helper_->WaitForToken(token);
}

void GLES2Implementation::TexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  // TODO(gman): Switch to use buckets alwayst or at least if no room in shared
  //    memory.
  uint32 pixels_size = GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_);
  memcpy(shared_memory_.GetAddress(0), pixels, pixels_size);
  helper_->TexImage2D(
      target, level, internalformat, width, height, border, format, type,
      shared_memory_.GetId(), 0);
  int32 token = helper_->InsertToken();
  helper_->WaitForToken(token);
}

void GLES2Implementation::TexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels) {
  // TODO(gman): Switch to use buckets alwayst or at least if no room in shared
  //    memory.
  uint32 pixels_size = GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_);
  memcpy(shared_memory_.GetAddress(0), pixels, pixels_size);
  helper_->TexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type,
      shared_memory_.GetId(), 0);
  int32 token = helper_->InsertToken();
  helper_->WaitForToken(token);
}


}  // namespace gles2
}  // namespace command_buffer


