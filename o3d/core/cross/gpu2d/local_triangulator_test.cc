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

// Tests for the local triangulator.

#include "core/cross/gpu2d/cubic_math_utils.h"
#include "core/cross/gpu2d/local_triangulator.h"
#include "gtest/gtest.h"

namespace o3d {
namespace gpu2d {

using cubic::ApproxEqual;

namespace {

// Sets up control point vertices for (approximately) a serpentine
// curve.
void SetupSerpentineVertices(LocalTriangulator* triangulator) {
  triangulator->get_vertex(0)->Set(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  triangulator->get_vertex(1)->Set(75.0f, 25.0f, 0.0f, 0.0f, 0.0f);
  triangulator->get_vertex(2)->Set(25.0f, 75.0f, 0.0f, 0.0f, 0.0f);
  triangulator->get_vertex(3)->Set(100.0f, 100.0f, 0.0f, 0.0f, 0.0f);
}

}  // anonymous namespace

TEST(LocalTriangulatorTest, TestShortestEdgeGuarantee) {
  LocalTriangulator triangulator;
  SetupSerpentineVertices(&triangulator);
  triangulator.Triangulate(false, false);
  EXPECT_EQ(triangulator.num_triangles(), 2);
  // It would be an error if the edge from (0, 0) to (100, 100) was
  // contained in either of the two triangles.
  for (int i = 0; i < triangulator.num_triangles(); i++) {
    LocalTriangulator::Triangle* triangle = triangulator.get_triangle(i);
    bool has_0_0 = false;
    bool has_100_100 = false;
    for (int j = 0; j < 3; j++) {
      LocalTriangulator::Vertex* vertex = triangle->get_vertex(j);
      if (ApproxEqual(vertex->x(), 0) && ApproxEqual(vertex->y(), 0)) {
        has_0_0 = true;
      }
      if (ApproxEqual(vertex->x(), 100) && ApproxEqual(vertex->y(), 100)) {
        has_100_100 = true;
      }
    }
    EXPECT_FALSE(has_0_0 && has_100_100);
  }
}

TEST(LocalTriangulatorTest, TestInteriorVertexGuarantee) {
  LocalTriangulator triangulator;
  triangulator.get_vertex(0)->Set(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  triangulator.get_vertex(1)->Set(100.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  triangulator.get_vertex(2)->Set(50.0f, 50.0f, 0.0f, 0.0f, 0.0f);
  triangulator.get_vertex(3)->Set(50.0f, 100.0f, 0.0f, 0.0f, 0.0f);
  triangulator.Triangulate(false, false);
  EXPECT_EQ(triangulator.num_triangles(), 3);
}

TEST(LocalTriangulatorTest, TestInteriorVertexComputationFillingRightSide) {
  LocalTriangulator triangulator;
  SetupSerpentineVertices(&triangulator);
  triangulator.Triangulate(true, true);
  EXPECT_EQ(triangulator.num_triangles(), 2);
  // In this configuration, vertex (75, 25) should be among the
  // interior vertices, and vertex (25, 75) should not.
  bool found_correct_interior_vertex = false;
  for (int i = 0; i < triangulator.num_interior_vertices(); i++) {
    LocalTriangulator::Vertex* vertex = triangulator.get_interior_vertex(i);
    if (ApproxEqual(vertex->x(), 75) && ApproxEqual(vertex->y(), 25)) {
      found_correct_interior_vertex = true;
    }
  }
  EXPECT_TRUE(found_correct_interior_vertex);
  for (int i = 0; i < triangulator.num_interior_vertices(); i++) {
    LocalTriangulator::Vertex* vertex = triangulator.get_interior_vertex(i);
    EXPECT_FALSE(ApproxEqual(vertex->x(), 25) && ApproxEqual(vertex->y(), 75));
  }
}

TEST(LocalTriangulatorTest, TestInteriorVertexComputationFillingLeftSide) {
  LocalTriangulator triangulator;
  SetupSerpentineVertices(&triangulator);
  triangulator.Triangulate(true, false);
  EXPECT_EQ(triangulator.num_triangles(), 2);
  // In this configuration, vertex (25, 75) should be among the
  // interior vertices, and vertex (75, 25) should not.
  bool found_correct_interior_vertex = false;
  for (int i = 0; i < triangulator.num_interior_vertices(); i++) {
    LocalTriangulator::Vertex* vertex = triangulator.get_interior_vertex(i);
    if (ApproxEqual(vertex->x(), 25) && ApproxEqual(vertex->y(), 75)) {
      found_correct_interior_vertex = true;
    }
  }
  EXPECT_TRUE(found_correct_interior_vertex);
  for (int i = 0; i < triangulator.num_interior_vertices(); i++) {
    LocalTriangulator::Vertex* vertex = triangulator.get_interior_vertex(i);
    EXPECT_FALSE(ApproxEqual(vertex->x(), 75) && ApproxEqual(vertex->y(), 25));
  }
}

}  // namespace gpu2d
}  // namespace o3d

