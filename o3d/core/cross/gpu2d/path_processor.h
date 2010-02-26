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

// The main entry point for Loop and Blinn's GPU accelerated curve
// rendering algorithm.

#ifndef O3D_CORE_CROSS_GPU2D_PATH_PROCESSOR_H_
#define O3D_CORE_CROSS_GPU2D_PATH_PROCESSOR_H_

#include <vector>

#include "base/basictypes.h"

class SkPath;

namespace o3d {
namespace gpu2d {

class Arena;
class Contour;
class PathCache;
class Segment;

// The PathProcessor turns an SkPath (assumed to contain one or more
// closed regions) into a set of exterior and interior triangles,
// stored in the PathCache. The exterior triangles have associated 3D
// texture coordinates which are used to evaluate the curve's
// inside/outside function on a per-pixel basis. The interior
// triangles are filled with 100% opacity.
//
// Note that the fill style and management of multiple layers are
// separate concerns, handled at a higher level with shaders and
// polygon offsets.
class PathProcessor {
 public:
  PathProcessor();
  explicit PathProcessor(Arena* arena);
  ~PathProcessor();

  // Transforms the given path into a triangle mesh for rendering
  // using Loop and Blinn's shader, placing the result into the given
  // PathCache.
  void Process(const SkPath& path, PathCache* cache);

  // Enables or disables verbose logging in debug mode.
  void set_verbose_logging(bool on_or_off);

 private:
  // Builds a list of contours for the given path.
  void BuildContours(const SkPath& path);

  // Determines whether the left or right side of each contour should
  // be filled.
  void DetermineSidesToFill();

  // Determines whether the given (closed) contour is oriented
  // clockwise or counterclockwise.
  void DetermineOrientation(Contour* contour);

  // Subdivides the curves so that there are no overlaps of the
  // triangles associated with the curves' control points.
  void SubdivideCurves();

  // Helper function used during curve subdivision.
  void ConditionallySubdivide(Segment* seg,
                              std::vector<Segment*>* next_segments);

  // Tessellates the interior regions of the contours.
  void TessellateInterior(PathCache* cache);

  // For debugging the orientation computation. Returns all of the
  // segments overlapping the given Y coordinate.
  std::vector<Segment*> AllSegmentsOverlappingY(float y);

  // For debugging the curve subdivision algorithm. Subdivides the
  // curves using an alternate, slow (O(n^3)) algorithm.
  void SubdivideCurvesSlow();

  // Arena from which to allocate temporary objects.
  Arena* arena_;

  // Whether to delete the arena upon deletion of the PathProcessor.
  bool should_delete_arena_;

  // The contours described by the path.
  std::vector<Contour*> contours_;

  // Whether or not to perform verbose logging in debug mode.
  bool verbose_logging_;

  DISALLOW_COPY_AND_ASSIGN(PathProcessor);
};

}  // namespace gpu2d
}  // namespace o3d

#endif  // O3D_CORE_CROSS_GPU2D_PATH_PROCESSOR_H_

