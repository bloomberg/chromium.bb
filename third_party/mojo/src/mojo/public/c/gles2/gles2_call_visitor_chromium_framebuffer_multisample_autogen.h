// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

VISIT_GL_CALL(
    BlitFramebufferCHROMIUM,
    void,
    (GLint srcX0,
     GLint srcY0,
     GLint srcX1,
     GLint srcY1,
     GLint dstX0,
     GLint dstY0,
     GLint dstX1,
     GLint dstY1,
     GLbitfield mask,
     GLenum filter),
    (srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter))
VISIT_GL_CALL(RenderbufferStorageMultisampleCHROMIUM,
              void,
              (GLenum target,
               GLsizei samples,
               GLenum internalformat,
               GLsizei width,
               GLsizei height),
              (target, samples, internalformat, width, height))
