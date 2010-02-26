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

#ifndef O3D_CORE_CROSS_PROCESSED_PATH_H_
#define O3D_CORE_CROSS_PROCESSED_PATH_H_

#include "core/cross/object_base.h"
#include "core/cross/gpu2d/path_cache.h"
#include "third_party/skia/include/core/SkPath.h"

namespace o3d {

class Field;

// This class is the link between the generic curve processing code in
// this directory and O3D. It runs the PathProcessor on a set of
// curves and returns the resulting triangles' vertices and texture
// coordinates in Fields which can be incorporated into primitives in
// the scene graph.
class ProcessedPath : public ObjectBase {
 public:
  typedef SmartPointer<ProcessedPath> Ref;

  virtual ~ProcessedPath();

  // Clears out all of the curve segments that have been added to this
  // path.
  void Clear();

  // Moves the pen to the given absolute X,Y coordinates. If a contour
  // isn't currently open on this path, one is opened.
  void MoveTo(float x, float y);

  // Draws a line from the current coordinates to the given absolute
  // X,Y coordinates.
  void LineTo(float x, float y);

  // Draws a quadratic curve from the current coordinates through the
  // given control point and end point, specified in absolute
  // coordinates.
  void QuadraticTo(float cx, float cy, float x, float y);

  // Draws a cubic curve from the current coordinates through the
  // given control points and end point, specified in absolute
  // coordinates.
  void CubicTo(float c0x, float c0y,
               float c1x, float c1y,
               float x, float y);

  // Closes the currently open contour on this path.
  void Close();

  // Creates the triangle mesh which will render the given curve
  // segments. There are two regions: exterior and interior. The
  // exterior region covers the portions containing the curve
  // segments. It has two associated fields: a 2D floating point field
  // for the positions, and a 3D floating point field for the
  // Loop/Blinn texture coordinates. The interior region has one 2D
  // floating point field for the positions. The contents of the
  // fields are organized for rendering as non-indexed triangles.
  void CreateMesh(Field* exterior_positions,
                  Field* exterior_texture_coordinates,
                  Field* interior_positions);

 private:
  explicit ProcessedPath(ServiceLocator* service_locator);

  void SetFields(const float* positions,
                 Field* position_field,
                 const float* texture_coordinates,
                 Field* texture_coordinate_field,
                 unsigned int num_vertices);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  SkPath path;
  gpu2d::PathCache cache;

  O3D_DECL_CLASS(ProcessedPath, ObjectBase);
  DISALLOW_COPY_AND_ASSIGN(ProcessedPath);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_PROCESSED_PATH_H_

