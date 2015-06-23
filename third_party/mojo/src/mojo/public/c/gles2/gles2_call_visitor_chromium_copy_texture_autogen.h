// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

VISIT_GL_CALL(CopyTextureCHROMIUM,
              void,
              (GLenum target,
               GLenum source_id,
               GLenum dest_id,
               GLint internalformat,
               GLenum dest_type,
               GLboolean unpack_flip_y,
               GLboolean unpack_premultiply_alpha,
               GLboolean unpack_unmultiply_alpha),
              (target,
               source_id,
               dest_id,
               internalformat,
               dest_type,
               unpack_flip_y,
               unpack_premultiply_alpha,
               unpack_unmultiply_alpha))
VISIT_GL_CALL(CopySubTextureCHROMIUM,
              void,
              (GLenum target,
               GLenum source_id,
               GLenum dest_id,
               GLint xoffset,
               GLint yoffset,
               GLint x,
               GLint y,
               GLsizei width,
               GLsizei height,
               GLboolean unpack_flip_y,
               GLboolean unpack_premultiply_alpha,
               GLboolean unpack_unmultiply_alpha),
              (target,
               source_id,
               dest_id,
               xoffset,
               yoffset,
               x,
               y,
               width,
               height,
               unpack_flip_y,
               unpack_premultiply_alpha,
               unpack_unmultiply_alpha))
