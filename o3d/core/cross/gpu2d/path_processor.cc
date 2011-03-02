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

#include "core/cross/gpu2d/path_processor.h"

#include <algorithm>

#include "base/logging.h"
#include "core/cross/gpu2d/arena.h"
#include "core/cross/gpu2d/cubic_classifier.h"
#include "core/cross/gpu2d/cubic_math_utils.h"
#include "core/cross/gpu2d/cubic_texture_coords.h"
#include "core/cross/gpu2d/interval_tree.h"
#include "core/cross/gpu2d/local_triangulator.h"
#include "core/cross/gpu2d/path_cache.h"
#include "third_party/skia/include/core/SkGeometry.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/glu/internal_glu.h"

namespace o3d {
namespace gpu2d {

class Contour;

//----------------------------------------------------------------------
// min / max helpers
//

template <typename T>
T min2(const T& v1, const T& v2) {
  return std::min(v1, v2);
}

template <typename T>
T max2(const T& v1, const T& v2) {
  return std::max(v1, v2);
}

template <typename T>
T min3(const T& v1, const T& v2, const T& v3) {
  return min2(min2(v1, v2), v3);
}

template <typename T>
T max3(const T& v1, const T& v2, const T& v3) {
  return max2(max2(v1, v2), v3);
}

template <typename T>
T min4(const T& v1, const T& v2, const T& v3, const T& v4) {
  return min2(min2(v1, v2), min2(v3, v4));
}

template <typename T>
T max4(const T& v1, const T& v2, const T& v3, const T& v4) {
  return max2(max2(v1, v2), max2(v3, v4));
}

//----------------------------------------------------------------------
// BBox
//

// Extremely simple bounding box class for Segments and Contours.
class BBox {
 public:
  BBox()
      : min_x_(0),
        min_y_(0),
        max_x_(0),
        max_y_(0) {
  }

  // Initializes the parameters of the bounding box.
  void Setup(float min_x,
             float min_y,
             float max_x,
             float max_y) {
    min_x_ = min_x;
    min_y_ = min_y;
    max_x_ = max_x;
    max_y_ = max_y;
  }

  // Initializes the bounding box to surround the given triangle.
  void Setup(LocalTriangulator::Triangle* triangle) {
    Setup(min3(triangle->get_vertex(0)->x(),
               triangle->get_vertex(1)->x(),
               triangle->get_vertex(2)->x()),
          min3(triangle->get_vertex(0)->y(),
               triangle->get_vertex(1)->y(),
               triangle->get_vertex(2)->y()),
          max3(triangle->get_vertex(0)->x(),
               triangle->get_vertex(1)->x(),
               triangle->get_vertex(2)->x()),
          max3(triangle->get_vertex(0)->y(),
               triangle->get_vertex(1)->y(),
               triangle->get_vertex(2)->y()));
  }

  // Initializes this bounding box to the contents of the other.
  void Setup(const BBox& bbox) {
    min_x_ = bbox.min_x();
    min_y_ = bbox.min_y();
    max_x_ = bbox.max_x();
    max_y_ = bbox.max_y();
  }

  // Extends this bounding box to surround itself and the other.
  void Extend(const BBox& other) {
    Setup(min2(min_x(), other.min_x()),
          min2(min_y(), other.min_y()),
          max2(max_x(), other.max_x()),
          max2(max_y(), other.max_y()));
  }

  float min_x() const { return min_x_; }
  float min_y() const { return min_y_; }
  float max_x() const { return max_x_; }
  float max_y() const { return max_y_; }

 private:
  float min_x_;
  float min_y_;
  float max_x_;
  float max_y_;
  DISALLOW_COPY_AND_ASSIGN(BBox);
};

// Suppport for logging BBoxes.
std::ostream& operator<<(std::ostream& ostr,  // NOLINT
                         const BBox& arg) {
  ostr << "[BBox min_x=" << arg.min_x()
       << " min_y=" << arg.min_y()
       << " max_x=" << arg.max_x()
       << " max_y=" << arg.max_y()
       << "]";
  return ostr;
}

//----------------------------------------------------------------------
// Segment
//

// Describes a segment of the path: either a cubic or a line segment.
// These are stored in a doubly linked list to speed up curve
// subdivision, which occurs due to either rendering artifacts in the
// loop case or due to overlapping triangles.
class Segment {
 public:
  // The kind of segment: cubic or line.
  enum Kind {
    kCubic,
    kLine
  };

  // No-argument constructor allows construction by the Arena class.
  Segment()
      : arena_(NULL),
        kind_(kCubic),
        prev_(NULL),
        next_(NULL),
        contour_(NULL),
        triangulator_(NULL),
        marked_for_subdivision_(false) {
  }

  // Initializer for cubic curve segments.
  void Setup(Arena* arena,
             Contour* contour,
             SkPoint cp0,
             SkPoint cp1,
             SkPoint cp2,
             SkPoint cp3) {
    arena_ = arena;
    contour_ = contour;
    kind_ = kCubic;
    points_[0] = cp0;
    points_[1] = cp1;
    points_[2] = cp2;
    points_[3] = cp3;
    ComputeBoundingBox();
  }

  // Initializer for line segments.
  void Setup(Arena* arena,
             Contour* contour,
             SkPoint p0,
             SkPoint p1) {
    arena_ = arena;
    contour_ = contour;
    kind_ = kLine;
    points_[0] = p0;
    points_[1] = p1;
    ComputeBoundingBox();
  }

  // Returns the kind of segment.
  Kind kind() const {
    return kind_;
  }

  // Returns the i'th control point, 0 <= i < 4.
  const SkPoint& get_point(int i) {
    DCHECK(i >= 0 && i < 4);
    return points_[i];
  }

  // Returns the next segment in the contour.
  Segment* next() const { return next_; }

  // Returns the previous segment in the contour.
  Segment* prev() const { return prev_; }

  // Sets the next segment in the contour.
  void set_next(Segment* next) { next_ = next; }

  // Sets the previous segment in the contour.
  void set_prev(Segment* prev) { prev_ = prev; }

  // The contour this segment belongs to.
  Contour* contour() const { return contour_; }

  // Subdivides the current segment at the given parameter value (0 <=
  // t <= 1) and replaces it with the two newly created Segments in
  // the linked list, if possible. Returns a pointer to the leftmost
  // Segment.
  Segment* Subdivide(float param) {
    SkPoint dst[7];
    SkChopCubicAt(points_, dst, param);
    Segment* left = arena_->Alloc<Segment>();
    Segment* right = arena_->Alloc<Segment>();
    left->Setup(arena_, contour_, dst[0], dst[1], dst[2], dst[3]);
    right->Setup(arena_, contour_, dst[3], dst[4], dst[5], dst[6]);
    left->set_next(right);
    right->set_prev(left);
    // Try to set up a link between "this->prev()" and "left".
    if (prev() != NULL) {
      left->set_prev(prev());
      prev()->set_next(left);
    }
    // Try to set up a link between "this->next()" and "right".
    Segment* n = next();
    if (n != NULL) {
      right->set_next(n);
      n->set_prev(right);
    }
    // Set up a link between "this" and "left"; this is only to
    // provide a certain amount of continuity during forward iteration.
    set_next(left);
    return left;
  }

  // Subdivides the current segment at the halfway point and replaces
  // it with the two newly created Segments in the linked list, if
  // possible. Returns a pointer to the leftmost Segment.
  Segment* Subdivide() {
    return Subdivide(0.5f);
  }

  // Returns the bounding box of this segment.
  const BBox& bbox() const {
    return bbox_;
  }

  // Computes the number of times a query line starting at the given
  // point and extending to x=+infinity crosses this segment. Outgoing
  // "ambiguous" argument indicates whether the query intersected an
  // endpoint or tangent point of the segment, indicating that another
  // query point is preferred.
  int NumCrossingsForXRay(SkPoint pt, bool* ambiguous) const {
    if (kind_ == kCubic) {
      // Should consider caching the monotonic cubics.
      return SkNumXRayCrossingsForCubic(pt, points_, ambiguous);
    } else {
      return SkXRayCrossesLine(pt, points_, ambiguous) ? 1 : 0;
    }
  }

  // Performs a local triangulation of the control points in this
  // segment. This operation only makes sense for cubic type segments.
  void Triangulate(bool compute_inside_edges,
                   CubicTextureCoords::Result* tex_coords);

  // Returns the number of control point triangles associated with
  // this segment.
  int num_triangles() const {
    if (!triangulator_)
      return 0;
    return triangulator_->num_triangles();
  }

  // Fetches the given control point triangle for this segment.
  LocalTriangulator::Triangle* get_triangle(int index) {
    DCHECK(triangulator_);
    return triangulator_->get_triangle(index);
  }

  // Number of vertices along the inside edge of this segment. This
  // can be called either for line or cubic type segments.
  int num_interior_vertices() const {
    if (kind_ == kCubic) {
      if (triangulator_) {
        return triangulator_->num_interior_vertices();
      } else {
        return 0;
      }
    }

    return 2;
  }

  // Returns the given interior vertex, 0 <= index < num_interior_vertices().
  SkPoint get_interior_vertex(int index) const {
    DCHECK(index >= 0 && index < num_interior_vertices());
    if (kind_ == kCubic) {
      SkPoint res = { 0 };
      if (triangulator_) {
        LocalTriangulator::Vertex* vertex =
            triangulator_->get_interior_vertex(index);
        if (vertex)
          res.set(SkFloatToScalar(vertex->x()),
                  SkFloatToScalar(vertex->y()));
      }
      return res;
    }

    return points_[index];
  }

  // State to assist with curve subdivision.
  bool marked_for_subdivision() {
    return marked_for_subdivision_;
  }

  // State to assist with curve subdivision.
  void set_marked_for_subdivision(bool marked_for_subdivision) {
    marked_for_subdivision_ = marked_for_subdivision;
  }

 private:
  // Computes the bounding box of this Segment.
  void ComputeBoundingBox() {
    switch (kind_) {
      case kCubic:
        bbox_.Setup(SkScalarToFloat(min4(points_[0].fX,
                                         points_[1].fX,
                                         points_[2].fX,
                                         points_[3].fX)),
                    SkScalarToFloat(min4(points_[0].fY,
                                         points_[1].fY,
                                         points_[2].fY,
                                         points_[3].fY)),
                    SkScalarToFloat(max4(points_[0].fX,
                                         points_[1].fX,
                                         points_[2].fX,
                                         points_[3].fX)),
                    SkScalarToFloat(max4(points_[0].fY,
                                         points_[1].fY,
                                         points_[2].fY,
                                         points_[3].fY)));
        break;

      case kLine:
        bbox_.Setup(SkScalarToFloat(min2(points_[0].fX,
                                         points_[1].fX)),
                    SkScalarToFloat(min2(points_[0].fY,
                                         points_[1].fY)),
                    SkScalarToFloat(max2(points_[0].fX,
                                         points_[1].fX)),
                    SkScalarToFloat(max2(points_[0].fY,
                                         points_[1].fY)));
        break;

      default:
        NOTREACHED();
        break;
    }
  }

  Arena* arena_;
  Kind kind_;
  SkPoint points_[4];
  Segment* prev_;
  Segment* next_;
  Contour* contour_;
  BBox bbox_;
  LocalTriangulator* triangulator_;
  bool marked_for_subdivision_;

  DISALLOW_COPY_AND_ASSIGN(Segment);
};

// Suppport for logging Segments.
std::ostream& operator<<(std::ostream& ostr,  // NOLINT
                         const Segment& arg) {
  ostr << "[Segment kind=";
  if (arg.kind() == Segment::kLine) {
    ostr << "line";
  } else {
    ostr << "cubic";
  }
  ostr << " bbox=" << arg.bbox() << "]";
  return ostr;
}

//----------------------------------------------------------------------
// Contour
//

// Describes a closed contour of the path.
class Contour {
 public:
  Contour() {
    first_ = &sentinel_;
    first_->set_next(first_);
    first_->set_prev(first_);
    ccw_ = true;
    bbox_dirty_ = false;
    fill_right_side_ = true;
  }

  // Adds a segment to this contour.
  void Add(Segment* segment) {
    if (first_ == &sentinel_) {
      // First element is the sentinel. Replace it with the incoming
      // segment.
      segment->set_next(first_);
      segment->set_prev(first_);
      first_->set_next(segment);
      first_->set_prev(segment);
      first_ = segment;
    } else {
      // first_->prev() is the sentinel.
      Segment* sentinel = first_->prev();
      Segment* last = sentinel->prev();
      last->set_next(segment);
      segment->set_prev(last);
      segment->set_next(sentinel);
      sentinel->set_prev(segment);
    }
    bbox_dirty_ = true;
  }

  // Subdivides the given segment at the given parametric value.
  // Returns a pointer to the first of the two portions of the
  // subdivided segment.
  Segment* Subdivide(Segment* segment, float param) {
    Segment* left = segment->Subdivide(param);
    if (first_ == segment)
      first_ = left;
    return left;
  }

  // Subdivides the given segment at the halfway point. Returns a
  // pointer to the first of the two portions of the subdivided
  // segment.
  Segment* Subdivide(Segment* segment) {
    Segment* left = segment->Subdivide();
    if (first_ == segment)
      first_ = left;
    return left;
  }

  // Returns the first segment in the contour for iteration.
  Segment* begin() const {
    return first_;
  }

  // Returns the last segment in the contour for iteration. Callers
  // should not iterate over this segment. In other words:
  //  for (Segment* cur = contour->begin();
  //       cur != contour->end();
  //       cur = cur->next()) {
  //    // .. process cur ...
  //  }
  Segment* end() const {
    return first_->prev();
  }

  // Returns whether this contour is oriented counterclockwise.
  bool ccw() const {
    return ccw_;
  }

  void set_ccw(bool ccw) {
    ccw_ = ccw;
  }

  // Returns the bounding box of this contour.
  const BBox& bbox() const {
    if (bbox_dirty_) {
      bool first = true;
      for (Segment* cur = begin(); cur != end(); cur = cur->next()) {
        if (first) {
          bbox_.Setup(cur->bbox());
        } else {
          bbox_.Extend(cur->bbox());
        }
        first = false;
      }

      bbox_dirty_ = false;
    }
    return bbox_;
  }

  // Returns whether the right side of this contour is filled.
  bool fill_right_side() const { return fill_right_side_; }

  void set_fill_right_side(bool fill_right_side) {
    fill_right_side_ = fill_right_side;
  }

 private:
  // The start of the segment chain. The segments are kept in a
  // circular doubly linked list for rapid access to the beginning and
  // end.
  Segment* first_;

  // The sentinel element at the end of the chain, needed for
  // reasonable iteration semantics.
  Segment sentinel_;

  // Whether this contour is oriented counterclockwise.
  bool ccw_;

  // This contour's bounding box.
  mutable BBox bbox_;

  // Whether this contour's bounding box is dirty.
  mutable bool bbox_dirty_;

  // Whether we should fill the right (or left) side of this contour.
  bool fill_right_side_;

  DISALLOW_COPY_AND_ASSIGN(Contour);
};

//----------------------------------------------------------------------
// Segment
//

// Definition of Segment::Triangulate(), which must come after
// declaration of Contour.
void Segment::Triangulate(bool compute_inside_edges,
                          CubicTextureCoords::Result* tex_coords) {
  DCHECK(kind_ == kCubic);
  if (triangulator_ == NULL) {
    triangulator_ = arena_->Alloc<LocalTriangulator>();
  }
  triangulator_->Reset();
  for (int i = 0; i < 4; i++) {
    LocalTriangulator::Vertex* vertex = triangulator_->get_vertex(i);
    if (tex_coords) {
      vertex->Set(get_point(i).fX,
                  get_point(i).fY,
                  tex_coords->coords[i].getX(),
                  tex_coords->coords[i].getY(),
                  tex_coords->coords[i].getZ());
    } else {
      vertex->Set(get_point(i).fX,
                  get_point(i).fY,
                  // No texture coordinates yet
                  0, 0, 0);
    }
  }
  triangulator_->Triangulate(compute_inside_edges,
                             contour()->fill_right_side());
}

//----------------------------------------------------------------------
// PathProcessor
//

PathProcessor::PathProcessor()
    : arena_(new Arena()),
      should_delete_arena_(true),
      verbose_logging_(false) {
}

PathProcessor::PathProcessor(Arena* arena)
    : arena_(arena),
      should_delete_arena_(false),
      verbose_logging_(false) {
}

PathProcessor::~PathProcessor() {
  if (should_delete_arena_) {
    delete arena_;
  }
}

void PathProcessor::Process(const SkPath& path, PathCache* cache) {
  BuildContours(path);

  // Run plane-sweep algorithm to determine overlaps of control point
  // curves and subdivide curves appropriately.
  SubdivideCurves();

  // Determine orientations of countours. Based on orientation and the
  // number of curve crossings at a random point on the contour,
  // determine whether to fill the left or right side of the contour.
  DetermineSidesToFill();

  // Classify curves, compute texture coordinates and subdivide as
  // necessary to eliminate rendering artifacts. Do the final
  // triangulation of the curve segments, determining the path along
  // the interior of the shape.
  for (std::vector<Contour*>::iterator iter = contours_.begin();
       iter != contours_.end();
       iter++) {
    Contour* cur = *iter;
    for (Segment* seg = cur->begin(); seg != cur->end(); seg = seg->next()) {
      if (seg->kind() == Segment::kCubic) {
        CubicClassifier::Result classification =
            CubicClassifier::Classify(seg->get_point(0).fX,
                                      seg->get_point(0).fY,
                                      seg->get_point(1).fX,
                                      seg->get_point(1).fY,
                                      seg->get_point(2).fX,
                                      seg->get_point(2).fY,
                                      seg->get_point(3).fX,
                                      seg->get_point(3).fY);
        DLOG_IF(INFO, verbose_logging_) << "Classification:"
                                        << classification.curve_type();
        CubicTextureCoords::Result tex_coords;
        CubicTextureCoords::Compute(classification,
                                    cur->fill_right_side(),
                                    &tex_coords);
        if (tex_coords.has_rendering_artifact) {
          // TODO(kbr): split at the subdivision parameter value
          cur->Subdivide(seg);
          // Next iteration will handle the newly subdivided halves
        } else {
          if (!tex_coords.is_line_or_point) {
            seg->Triangulate(true, &tex_coords);
            for (int i = 0; i < seg->num_triangles(); i++) {
              LocalTriangulator::Triangle* triangle =
                  seg->get_triangle(i);
              for (int j = 0; j < 3; j++) {
                LocalTriangulator::Vertex* vert =
                    triangle->get_vertex(j);
                cache->AddVertex(vert->x(),
                                 vert->y(),
                                 vert->k(),
                                 vert->l(),
                                 vert->m());
              }
            }
#ifdef O3D_CORE_CROSS_GPU2D_PATH_CACHE_DEBUG_INTERIOR_EDGES
            // Show the end user the interior edges as well
            for (int i = 1; i < seg->num_interior_vertices(); i++) {
              SkPoint vert = seg->get_interior_vertex(i);
              // Duplicate previous vertex to be able to draw GL_LINES
              SkPoint prev = seg->get_interior_vertex(i - 1);
              cache->AddInteriorEdgeVertex(prev.fX, prev.fY);
              cache->AddInteriorEdgeVertex(vert.fX, vert.fY);
            }
#endif  // O3D_CORE_CROSS_GPU2D_PATH_CACHE_DEBUG_INTERIOR_EDGES
          }
        }
      }
    }
  }

  // Run the interior paths through a tessellation algorithm
  // supporting multiple contours.
  TessellateInterior(cache);
}

void PathProcessor::BuildContours(const SkPath& path) {
  // Clear out the contours
  contours_.clear();
  SkPath::Iter iter(path, false);
  SkPoint pts[4];
  SkPath::Verb verb;
  Contour* contour = NULL;
  SkPoint cur_pt = { 0 };
  SkPoint move_to_pt = { 0 };
  do {
    verb = iter.next(pts);
    if (verb != SkPath::kMove_Verb) {
      if (contour == NULL) {
        contour = arena_->Alloc<Contour>();
        contours_.push_back(contour);
      }
    }
    switch (verb) {
      case SkPath::kMove_Verb: {
        contour = arena_->Alloc<Contour>();
        contours_.push_back(contour);
        cur_pt = pts[0];
        move_to_pt = pts[0];
        DLOG_IF(INFO, verbose_logging_) << "MoveTo (" << pts[0].fX
                                        << ", " << pts[0].fY << ")";
        break;
      }
      case SkPath::kLine_Verb: {
        Segment* segment = arena_->Alloc<Segment>();
        if (iter.isCloseLine()) {
          segment->Setup(arena_, contour, cur_pt, pts[1]);
          DLOG_IF(INFO, verbose_logging_) << "CloseLineTo (" << cur_pt.fX
                                          << ", " << cur_pt.fY
                                          << "), (" << pts[1].fX
                                          << ", " << pts[1].fY << ")";
          contour->Add(segment);
          contour = NULL;
        } else {
          segment->Setup(arena_, contour, pts[0], pts[1]);
          DLOG_IF(INFO, verbose_logging_) << "LineTo (" << pts[0].fX
                                          << ", " << pts[0].fY
                                          << "), (" << pts[1].fX
                                          << ", " << pts[1].fY << ")";
          contour->Add(segment);
          cur_pt = pts[1];
        }
        break;
      }
      case SkPath::kQuad_Verb: {
        // Need to degree elevate the quadratic into a cubic
        SkPoint cubic[4];
        SkConvertQuadToCubic(pts, cubic);
        Segment* segment = arena_->Alloc<Segment>();
        segment->Setup(arena_, contour,
                       cubic[0], cubic[1], cubic[2], cubic[3]);
        DLOG_IF(INFO, verbose_logging_) << "Quad->CubicTo (" << cubic[0].fX
                                        << ", " << cubic[0].fY
                                        << "), (" << cubic[1].fX
                                        << ", " << cubic[1].fY
                                        << "), (" << cubic[2].fX
                                        << ", " << cubic[2].fY
                                        << "), (" << cubic[3].fX
                                        << ", " << cubic[3].fY << ")";
        contour->Add(segment);
        cur_pt = cubic[3];
        break;
      }
      case SkPath::kCubic_Verb: {
        Segment* segment = arena_->Alloc<Segment>();
        segment->Setup(arena_, contour, pts[0], pts[1], pts[2], pts[3]);
        DLOG_IF(INFO, verbose_logging_) << "CubicTo (" << pts[0].fX
                                        << ", " << pts[0].fY
                                        << "), (" << pts[1].fX
                                        << ", " << pts[1].fY
                                        << "), (" << pts[2].fX
                                        << ", " << pts[2].fY
                                        << "), (" << pts[3].fX
                                        << ", " << pts[3].fY << ")";
        contour->Add(segment);
        cur_pt = pts[3];
        break;
      }
      case SkPath::kClose_Verb: {
        Segment* segment = arena_->Alloc<Segment>();
        segment->Setup(arena_, contour, cur_pt, move_to_pt);
        DLOG_IF(INFO, verbose_logging_) << "Close (" << cur_pt.fX
                                        << ", " << cur_pt.fY
                                        << ") -> (" << move_to_pt.fX
                                        << ", " <<  move_to_pt.fY << ")";
        contour->Add(segment);
        contour = NULL;
      }
      default:
        break;
    }
  } while (verb != SkPath::kDone_Verb);
}

std::vector<Segment*> PathProcessor::AllSegmentsOverlappingY(float y) {
  std::vector<Segment*> res;
  for (std::vector<Contour*>::iterator iter = contours_.begin();
       iter != contours_.end();
       iter++) {
    Contour* cur = *iter;
    for (Segment* seg = cur->begin(); seg != cur->end(); seg = seg->next()) {
      const BBox& bbox = seg->bbox();
      if (bbox.min_y() <= y && y <= bbox.max_y()) {
        res.push_back(seg);
      }
    }
  }
  return res;
}

// Uncomment this to debug the orientation computation
// #define O3D_CORE_CROSS_GPU2D_PATH_PROCESSOR_DEBUG_ORIENTATION

void PathProcessor::DetermineSidesToFill() {
  // Loop and Blinn's algorithm can only easily emulate the even/odd
  // fill rule, and only for non-intersecting curves. We can determine
  // which side of each curve segment to fill based on its
  // clockwise/counterclockwise orientation and how many other
  // contours surround it.

  // To optimize the query of all curve segments intersecting a
  // horizontal line going to x=+infinity, we build up an interval
  // tree whose keys are the y extents of the segments.
  IntervalTree<float, Segment*> tree(arena_);
  typedef IntervalTree<float, Segment*>::IntervalType IntervalType;

  for (std::vector<Contour*>::iterator iter = contours_.begin();
       iter != contours_.end();
       iter++) {
    Contour* cur = *iter;
    DetermineOrientation(cur);
    for (Segment* seg = cur->begin(); seg != cur->end(); seg = seg->next()) {
      const BBox& bbox = seg->bbox();
      tree.Insert(tree.MakeInterval(bbox.min_y(), bbox.max_y(), seg));
    }
  }

  // Now iterate through the contours and pick a random segment (in
  // this case we use the first) and a random point on that segment.
  // Find all segments from other contours which intersect this one
  // and count the number of crossings a horizontal line to
  // x=+infinity makes with those contours. This combined with the
  // orientation of the curve tells us which side to fill -- again,
  // assuming an even/odd fill rule, which is all we can easily
  // handle.
  for (std::vector<Contour*>::iterator iter = contours_.begin();
       iter != contours_.end();
       iter++) {
    Contour* cur = *iter;

    bool ambiguous = true;
    int num_crossings = 0;

    // For each contour, attempt to find a point on the contour which,
    // when we cast an XRay, does not intersect the other contours at
    // an ambiguous point (the junction between two curves or at a
    // tangent point). Ambiguous points make the determination of
    // whether this contour is contained within another fragile. Note
    // that this loop is only an approximation to the selection of a
    // good casting point. We could as well evaluate a segment to
    // determine a point upon it.
    for (Segment* seg = cur->begin();
         ambiguous && seg != cur->end();
         seg = seg->next()) {
      num_crossings = 0;
      // We use a zero-sized vertical interval for the query.
      std::vector<IntervalType> overlaps =
          tree.AllOverlaps(IntervalType(SkScalarToFloat(seg->get_point(0).fY),
                                        SkScalarToFloat(seg->get_point(0).fY),
                                        NULL));
#if !defined(O3D_CORE_CROSS_GPU2D_PATH_PROCESSOR_DEBUG_ORIENTATION)
      for (std::vector<IntervalType>::iterator iter = overlaps.begin();
           iter != overlaps.end();
           iter++) {
        const IntervalType& interval = *iter;
        Segment* query_seg = interval.data();
        // Ignore segments coming from the same contour.
        if (query_seg->contour() != cur) {
          // Only perform queries that can affect the computation.
          const BBox& bbox = query_seg->contour()->bbox();
          if (seg->get_point(0).fX >= bbox.min_x() &&
              seg->get_point(0).fX <= bbox.max_x()) {
            num_crossings += query_seg->NumCrossingsForXRay(seg->get_point(0),
                                                            &ambiguous);
            if (ambiguous) {
              DLOG(INFO) << "Ambiguous intersection query at point ("
                         << seg->get_point(0).fX << ", "
                         << seg->get_point(0).fY << ")";
              DLOG(INFO) << "Query segment: " << *query_seg;
              break;  // Abort iteration over overlaps.
            }
          }
        }
      }
#endif  // !defined(O3D_CORE_CROSS_GPU2D_PATH_PROCESSOR_DEBUG_ORIENTATION)

#ifdef O3D_CORE_CROSS_GPU2D_PATH_PROCESSOR_DEBUG_ORIENTATION
      // For debugging
      std::vector<Segment*> slow_overlaps =
          AllSegmentsOverlappingY(seg->get_point(0).fY);
      if (overlaps.size() != slow_overlaps.size()) {
        DLOG(ERROR) << "for query point " << seg->get_point(0).fY << ":";
        DLOG(ERROR) << " overlaps:";
        for (size_t i = 0; i < overlaps.size(); i++) {
          DLOG(ERROR) << "  " << (i+1) << ": " << *overlaps[i].data();
        }
        DLOG(ERROR) << " slow_overlaps:";
        for (size_t i = 0; i < slow_overlaps.size(); i++) {
          DLOG(ERROR) << "  " << (i+1) << ": " << *slow_overlaps[i];
        }
      }
      DCHECK(overlaps.size() == slow_overlaps.size());
      for (std::vector<Segment*>::iterator iter = slow_overlaps.begin();
           iter != slow_overlaps.end();
           iter++) {
        Segment* query_seg = *iter;
        // Ignore segments coming from the same contour.
        if (query_seg->contour() != cur) {
          // Only perform queries that can affect the computation.
          const BBox& bbox = query_seg->contour()->bbox();
          if (seg->get_point(0).fX >= bbox.min_x() &&
              seg->get_point(0).fX <= bbox.max_x()) {
            num_crossings += query_seg->NumCrossingsForXRay(seg->get_point(0),
                                                            &ambiguous);
            if (ambiguous) {
              DLOG(INFO) << "Ambiguous intersection query at point ("
                         << seg->get_point(0).fX << ", "
                         << seg->get_point(0).fY << ")";
              DLOG(INFO) << "Query segment: " << *query_seg;
              break;  // Abort iteration over overlaps.
            }
          }
        }
      }
#endif  // O3D_CORE_CROSS_GPU2D_PATH_PROCESSOR_DEBUG_ORIENTATION
    }  // for (Segment* seg = cur->begin(); ...

    if (cur->ccw()) {
      if (num_crossings & 1) {
        cur->set_fill_right_side(true);
      } else {
        cur->set_fill_right_side(false);
      }
    } else {
      if (num_crossings & 1) {
        cur->set_fill_right_side(false);
      } else {
        cur->set_fill_right_side(true);
      }
    }
  }
}

void PathProcessor::DetermineOrientation(Contour* contour) {
  // Determine signed area of the polygon represented by the points
  // along the segments. Consider this an approximation to the true
  // orientation of the polygon; it probably won't handle
  // self-intersecting curves correctly.
  //
  // There is also a pretty basic assumption here that the contour is
  // closed.
  float signed_area = 0;
  for (Segment* seg = contour->begin();
       seg != contour->end();
       seg = seg->next()) {
    int limit = (seg->kind() == Segment::kCubic) ? 4 : 2;
    for (int i = 1; i < limit; i++) {
      const SkPoint& prev_point = seg->get_point(i - 1);
      const SkPoint& point = seg->get_point(i);
      float cur_area = prev_point.fX * point.fY - prev_point.fY * point.fX;
      DLOG_IF(INFO, verbose_logging_) << "Adding to signed area ("
                                      << prev_point.fX << ", "
                                      << prev_point.fY << ") -> ("
                                      << point.fX << ", "
                                      << point.fY << ") = "
                                      << cur_area;
      signed_area += cur_area;
    }
  }

  if (signed_area > 0)
    contour->set_ccw(true);
  else
    contour->set_ccw(false);
}

//----------------------------------------------------------------------
// Classes and typedefs needed for curve subdivision.
// Unfortunately it appears we can't scope these within the
// SubdivideCurves() method itself, because templates then fail to
// instantiate.

// The user data which is placed in the IntervalTree.
struct SweepData {
  SweepData()
      : triangle(NULL),
        segment(NULL) {
  }

  // The triangle this interval is associated with
  LocalTriangulator::Triangle* triangle;
  // The segment the triangle is associated with
  Segment* segment;
};

typedef IntervalTree<float, SweepData*> SweepTree;
typedef SweepTree::IntervalType SweepInterval;

// The entry / exit events which occur at the minimum and maximum x
// coordinates of the control point triangles' bounding boxes.
//
// Note that this class requires its copy constructor and assignment
// operator since it needs to be stored in a std::vector.
class SweepEvent {
 public:
  SweepEvent()
      : x_(0),
        entry_(false),
        interval_(0, 0, NULL) {
  }

  // Initializes the SweepEvent.
  void Setup(float x, bool entry, SweepInterval interval) {
    x_ = x;
    entry_ = entry;
    interval_ = interval;
  }

  float x() const { return x_; }
  bool entry() const { return entry_; }
  const SweepInterval& interval() const { return interval_; }

  bool operator<(const SweepEvent& other) const {
    return x_ < other.x_;
  }

 private:
  float x_;
  bool entry_;
  SweepInterval interval_;
};

namespace {

bool TrianglesOverlap(LocalTriangulator::Triangle* t0,
                      LocalTriangulator::Triangle* t1) {
  LocalTriangulator::Vertex* t0v0 = t0->get_vertex(0);
  LocalTriangulator::Vertex* t0v1 = t0->get_vertex(1);
  LocalTriangulator::Vertex* t0v2 = t0->get_vertex(2);
  LocalTriangulator::Vertex* t1v0 = t1->get_vertex(0);
  LocalTriangulator::Vertex* t1v1 = t1->get_vertex(1);
  LocalTriangulator::Vertex* t1v2 = t1->get_vertex(2);
  return cubic::TrianglesOverlap(t0v0->x(), t0v0->y(),
                                 t0v1->x(), t0v1->y(),
                                 t0v2->x(), t0v2->y(),
                                 t1v0->x(), t1v0->y(),
                                 t1v1->x(), t1v1->y(),
                                 t1v2->x(), t1v2->y());
}

}  // anonymous namespace

void PathProcessor::SubdivideCurves() {
  // We need to determine all overlaps of all control point triangles
  // (from different segments, not the same segment) and, if any
  // exist, subdivide the associated curves.
  //
  // The plane-sweep algorithm determines all overlaps of a set of
  // rectangles in the 2D plane. Our problem maps very well to this
  // algorithm and significantly reduces the complexity compared to a
  // naive implementation.
  //
  // Each bounding box of a control point triangle is converted into
  // an "entry" event at its smallest X coordinate and an "exit" event
  // at its largest X coordinate. Each event has an associated
  // one-dimensional interval representing the Y span of the bounding
  // box. We sort these events by increasing X coordinate. We then
  // iterate through them. For each entry event we add the interval to
  // a side interval tree, and query this tree for overlapping
  // intervals. Any overlapping interval corresponds to an overlapping
  // bounding box. For each exit event we remove the associated
  // interval from the interval tree.

  std::vector<Segment*> cur_segments;
  std::vector<Segment*> next_segments;

  // Start things off by considering all of the segments
  for (std::vector<Contour*>::iterator iter = contours_.begin();
       iter != contours_.end();
       iter++) {
    Contour* cur = *iter;
    for (Segment* seg = cur->begin(); seg != cur->end(); seg = seg->next()) {
      if (seg->kind() == Segment::kCubic) {
        seg->Triangulate(false, NULL);
        cur_segments.push_back(seg);
      }
    }
  }

  // Subdivide curves at most this many times
  const int kMaxIter = 5;
  std::vector<SweepInterval> overlaps;

  for (int cur_iter = 0; cur_iter < kMaxIter; ++cur_iter) {
    if (cur_segments.empty()) {
      // Done
      break;
    }

    std::vector<SweepEvent> events;
    SweepTree tree(arena_);
    for (std::vector<Segment*>::iterator iter = cur_segments.begin();
         iter != cur_segments.end();
         iter++) {
      Segment* seg = *iter;
      if (seg->kind() == Segment::kCubic) {
        for (int i = 0; i < seg->num_triangles(); i++) {
          LocalTriangulator::Triangle* triangle = seg->get_triangle(i);
          BBox bbox;
          bbox.Setup(triangle);
          // Ignore zero-width triangles to avoid issues with
          // coincident entry and exit events for the same triangle
          if (bbox.max_x() > bbox.min_x()) {
            SweepData* data = arena_->Alloc<SweepData>();
            data->triangle = triangle;
            data->segment = seg;
            SweepInterval interval =
                tree.MakeInterval(bbox.min_y(), bbox.max_y(), data);
            // Add entry and exit events
            SweepEvent event;
            event.Setup(bbox.min_x(), true, interval);
            events.push_back(event);
            event.Setup(bbox.max_x(), false, interval);
            events.push_back(event);
          }
        }
      }
    }

    // Sort events by increasing X coordinate
    std::sort(events.begin(), events.end());

    // Now iterate through the events
    for (std::vector<SweepEvent>::iterator iter = events.begin();
         iter != events.end();
         iter++) {
      SweepEvent event = *iter;
      if (event.entry()) {
        // Add this interval into the tree
        tree.Insert(event.interval());
        // See whether the associated segment has been subdivided yet
        if (!event.interval().data()->segment->marked_for_subdivision()) {
          // Query the tree
          overlaps.clear();
          tree.AllOverlaps(event.interval(), overlaps);
          // Now see exactly which triangles overlap this one
          for (std::vector<SweepInterval>::iterator iter = overlaps.begin();
               iter != overlaps.end();
               iter++) {
            SweepInterval overlap = *iter;
            // Only pay attention to overlaps from a different Segment
            if (event.interval().data()->segment != overlap.data()->segment) {
              // See whether the triangles actually overlap
              if (TrianglesOverlap(event.interval().data()->triangle,
                                   overlap.data()->triangle)) {
                // Actually subdivide the segments.
                // Each one might already have been subdivided.
                Segment* seg = event.interval().data()->segment;
                ConditionallySubdivide(seg, &next_segments);
                seg = overlap.data()->segment;
                ConditionallySubdivide(seg, &next_segments);
              }
            }
          }
        }
      } else {
        // Remove this interval from the tree
        tree.Delete(event.interval());
      }
    }

    cur_segments = next_segments;
    next_segments.clear();
  }
}

void PathProcessor::ConditionallySubdivide(
    Segment* seg, std::vector<Segment*>* next_segments) {
  if (!seg->marked_for_subdivision()) {
    seg->set_marked_for_subdivision(true);
    Segment* next = seg->contour()->Subdivide(seg);
    // Triangulate the newly subdivided segments.
    next->Triangulate(false, NULL);
    next->next()->Triangulate(false, NULL);
    // Add them for the next iteration.
    next_segments->push_back(next);
    next_segments->push_back(next->next());
  }
}

void PathProcessor::SubdivideCurvesSlow() {
  // Alternate, significantly slower algorithm for curve subdivision
  // for use in debugging.
  std::vector<Segment*> cur_segments;
  std::vector<Segment*> next_segments;

  // Start things off by considering all of the segments
  for (std::vector<Contour*>::iterator iter = contours_.begin();
       iter != contours_.end();
       iter++) {
    Contour* cur = *iter;
    for (Segment* seg = cur->begin(); seg != cur->end(); seg = seg->next()) {
      if (seg->kind() == Segment::kCubic) {
        seg->Triangulate(false, NULL);
        cur_segments.push_back(seg);
      }
    }
  }

  // Subdivide curves at most this many times
  const int kMaxIter = 5;

  for (int cur_iter = 0; cur_iter < kMaxIter; ++cur_iter) {
    if (cur_segments.empty()) {
      // Done
      break;
    }

    for (std::vector<Segment*>::iterator iter = cur_segments.begin();
         iter != cur_segments.end();
         iter++) {
      Segment* seg = *iter;
      if (seg->kind() == Segment::kCubic) {
        for (std::vector<Segment*>::iterator iter2 = cur_segments.begin();
             iter2 != cur_segments.end();
             iter2++) {
          Segment* seg2 = *iter2;
          if (seg2->kind() == Segment::kCubic &&
              seg != seg2) {
            for (int i = 0; i < seg->num_triangles(); i++) {
              LocalTriangulator::Triangle* triangle = seg->get_triangle(i);
              for (int j = 0; j < seg2->num_triangles(); j++) {
                LocalTriangulator::Triangle* triangle2 = seg2->get_triangle(j);
                if (TrianglesOverlap(triangle, triangle2)) {
                  ConditionallySubdivide(seg, &next_segments);
                  ConditionallySubdivide(seg2, &next_segments);
                }
              }
            }
          }
        }
      }
    }

    cur_segments = next_segments;
    next_segments.clear();
  }
}

//----------------------------------------------------------------------
// Structures and callbacks for tessellation of the interior region of
// the contours.

// The user data for the GLU tessellator.
struct TessellationState {
  PathCache* cache;
  std::vector<void*> allocated_pointers;
};

namespace {

void VertexCallback(void* vertex_data, void* data) {
  TessellationState* state = static_cast<TessellationState*>(data);
  PathCache* cache = state->cache;
  GLdouble* location = static_cast<GLdouble*>(vertex_data);
  cache->AddInteriorVertex(static_cast<float>(location[0]),
                           static_cast<float>(location[1]));
}

void CombineCallback(GLdouble coords[3], void* vertex_data[4],
                     GLfloat weight[4], void** out_data,
                     void* polygon_data) {
  TessellationState* state = static_cast<TessellationState*>(polygon_data);
  GLdouble* out_vertex = static_cast<GLdouble*>(malloc(3 * sizeof(GLdouble)));
  state->allocated_pointers.push_back(out_vertex);
  out_vertex[0] = coords[0];
  out_vertex[1] = coords[1];
  out_vertex[2] = coords[2];
  *out_data = out_vertex;
}

void EdgeFlagCallback(GLboolean flag) {
  // No-op just to prevent triangle strips and fans from being passed
  // to us
}

}  // anonymous namespace

void PathProcessor::TessellateInterior(PathCache* cache) {
  // Because the GLU tessellator requires its input in
  // double-precision format, we need to make a separate copy of the
  // data.
  std::vector<GLdouble> vertex_data;
  std::vector<size_t> contour_endings;
  // For avoiding adding coincident vertices.
  float cur_x = 0, cur_y = 0;
  for (std::vector<Contour*>::iterator iter = contours_.begin();
       iter != contours_.end();
       iter++) {
    Contour* cur = *iter;
    bool first = true;
    for (Segment* seg = cur->begin(); seg != cur->end(); seg = seg->next()) {
      int num_interior_vertices = seg->num_interior_vertices();
      for (int i = 0; i < num_interior_vertices - 1; i++) {
        SkPoint point = seg->get_interior_vertex(i);
        if (first) {
          first = false;
          vertex_data.push_back(point.fX);
          vertex_data.push_back(point.fY);
          vertex_data.push_back(0);
          cur_x = point.fX;
          cur_y = point.fY;
        } else if (point.fX != cur_x || point.fY != cur_y)  {
          vertex_data.push_back(point.fX);
          vertex_data.push_back(point.fY);
          vertex_data.push_back(0);
          cur_x = point.fX;
          cur_y = point.fY;
        }
      }
    }
    contour_endings.push_back(vertex_data.size());
  }
  // Now that we have all of the vertex data in a stable location in
  // memory, call the tessellator.
  GLUtesselator* tess = internal_gluNewTess();
  TessellationState state;
  state.cache = cache;
  internal_gluTessCallback(tess, GLU_TESS_VERTEX_DATA,
      reinterpret_cast<GLvoid (*)()>(VertexCallback));
  internal_gluTessCallback(tess, GLU_TESS_COMBINE_DATA,
      reinterpret_cast<GLvoid (*)()>(CombineCallback));
  internal_gluTessCallback(tess, GLU_TESS_EDGE_FLAG,
      reinterpret_cast<GLvoid (*)()>(EdgeFlagCallback));
  internal_gluTessBeginPolygon(tess, &state);
  internal_gluTessBeginContour(tess);
  GLdouble* base = &vertex_data.front();
  int contour_idx = 0;
  for (size_t i = 0; i < vertex_data.size(); i += 3) {
    if (i == contour_endings[contour_idx]) {
      internal_gluTessEndContour(tess);
      internal_gluTessBeginContour(tess);
      ++contour_idx;
    }
    internal_gluTessVertex(tess, &base[i], &base[i]);
  }
  internal_gluTessEndContour(tess);
  internal_gluTessEndPolygon(tess);
  for (size_t i = 0; i < state.allocated_pointers.size(); i++) {
    free(state.allocated_pointers[i]);
  }
  internal_gluDeleteTess(tess);
}

}  // namespace gpu2d
}  // namespace o3d

