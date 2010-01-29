/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "base/string_util.h"
#include "core/cross/stream.h"
#include "core/cross/types.h"
#include "core/cross/gles2/utils_gles2.h"
#include "core/cross/gles2/gles2_headers.h"

// Required OpenGLES2 extensions:
// GL_ARB_vertex_buffer_object
// GL_ARB_vertex_program
// GL_ARB_texture_compression
// GL_EXT_texture_compression_dxt1

namespace o3d {

bool SemanticNameToSemantic(
    const String& name, Stream::Semantic* semantic, int* semantic_index) {
  struct NameToSemantic {
    const char* const name;
    size_t length;
    const Stream::Semantic semantic;
  };
  static const char kPosition[] = "POSITION";
  static const char kNormal[] = "NORMAL";
  static const char kTangent[] = "TANGENT";
  static const char kBinormal[] = "BINORMAL";
  static const char kColor[] = "COLOR";
  static const char kTexcoord[] = "TEXCOORD";

  static const NameToSemantic lookup[] = {
    { kPosition, sizeof(kPosition) - 1, Stream::POSITION, },
    { kNormal, sizeof(kNormal) - 1, Stream::NORMAL, },
    { kTangent, sizeof(kTangent) - 1, Stream::TANGENT, },
    { kBinormal, sizeof(kBinormal) - 1, Stream::BINORMAL, },
    { kColor, sizeof(kColor) - 1, Stream::COLOR, },
    { kTexcoord, sizeof(kTexcoord) - 1, Stream::TEXCOORD, },
  };
  for (unsigned ii = 0; ii < ARRAYSIZE_UNSAFE(lookup); ++ii) {
    const NameToSemantic& info = lookup[ii];
    if (!base::strncasecmp(info.name, name.c_str(), info.length)) {
      *semantic = info.semantic;
      *semantic_index = atoi(name.c_str() + info.length);
      return true;
    }
  }
  return false;
}

}  // namespace o3d
