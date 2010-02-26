/*
 * Copyright 2010, Google Inc.
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

// This file contains the definition of ProcessedPath.

#include "core/cross/processed_path.h"

#include "core/cross/buffer.h"
#include "core/cross/field.h"
#include "core/cross/gpu2d/path_processor.h"

namespace o3d {

O3D_DEFN_CLASS(ProcessedPath, ObjectBase);

ProcessedPath::ProcessedPath(ServiceLocator* service_locator)
    : ObjectBase(service_locator) {
}

ObjectBase::Ref ProcessedPath::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(
      ProcessedPath::Ref(new ProcessedPath(service_locator)));
}

ProcessedPath::~ProcessedPath() {
}

void ProcessedPath::Clear() {
  path.reset();
}

void ProcessedPath::MoveTo(float x, float y) {
  path.moveTo(SkFloatToScalar(x), SkFloatToScalar(y));
}

void ProcessedPath::LineTo(float x, float y) {
  path.lineTo(SkFloatToScalar(x), SkFloatToScalar(y));
}

void ProcessedPath::QuadraticTo(float cx, float cy, float x, float y) {
  path.quadTo(SkFloatToScalar(cx),
              SkFloatToScalar(cy),
              SkFloatToScalar(x),
              SkFloatToScalar(y));
}

void ProcessedPath::CubicTo(float c0x, float c0y,
                            float c1x, float c1y,
                            float x, float y) {
  path.cubicTo(SkFloatToScalar(c0x),
               SkFloatToScalar(c0y),
               SkFloatToScalar(c1x),
               SkFloatToScalar(c1y),
               SkFloatToScalar(x),
               SkFloatToScalar(y));
}

void ProcessedPath::Close() {
  path.close();
}

void ProcessedPath::CreateMesh(Field* exterior_positions,
                               Field* exterior_texture_coordinates,
                               Field* interior_positions) {
  gpu2d::PathProcessor processor;
  cache.Clear();
  processor.Process(path, &cache);
  // Copy the exterior vertices and texture coordinates into the
  // fields for the exterior triangles.
  SetFields(cache.vertices(), exterior_positions,
            cache.texcoords(), exterior_texture_coordinates,
            cache.num_vertices());
  // Copy the interior vertices into the field for the interior
  // triangles.
  SetFields(cache.interior_vertices(), interior_positions,
            NULL, NULL,
            cache.num_interior_vertices());
}

void ProcessedPath::SetFields(const float* positions,
                              Field* position_field,
                              const float* texture_coordinates,
                              Field* texture_coordinate_field,
                              unsigned int num_vertices) {
  // It may happen that there were no exterior or interior triangles.
  // In this case we allocate a single vertex in the fields to
  // indicate this to JavaScript since we can not allocate a
  // zero-sized buffer.
  DCHECK_NE(num_vertices, 1u);
  if (num_vertices == 0) {
    DCHECK_EQ(positions, static_cast<const float*>(NULL));
    DCHECK_EQ(texture_coordinates, static_cast<const float*>(NULL));
    num_vertices = 1;
  }
  Buffer* buffer = position_field->buffer();
  if (!buffer)
    return;
  if (buffer->num_elements() != num_vertices) {
    if (!buffer->AllocateElements(num_vertices)) {
      return;
    }
  }
  if (num_vertices == 1) {
    float tmp[2] = { 0 };
    position_field->SetFromFloats(tmp, 2, 0, 1);
  } else {
    position_field->SetFromFloats(positions, 2, 0, num_vertices);
  }

  // The texture coordinates are NULL for the interior triangles.
  if (texture_coordinate_field != NULL) {
    buffer = texture_coordinate_field->buffer();
    if (!buffer)
      return;
    if (buffer->num_elements() != num_vertices) {
      if (!buffer->AllocateElements(num_vertices)) {
        return;
      }
    }
    if (num_vertices == 1) {
      float tmp[3] = { 0 };
      texture_coordinate_field->SetFromFloats(tmp, 3, 0, 1);
    } else {
      texture_coordinate_field->SetFromFloats(texture_coordinates,
                                              3, 0, num_vertices);
    }
  }
}

}  // namespace o3d

