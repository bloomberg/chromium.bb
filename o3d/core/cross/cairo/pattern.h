/*
 * Copyright 2011, Google Inc.
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

// A Pattern is a container for pixel content for painting to a Layer.

#ifndef O3D_CORE_CROSS_CAIRO_PATTERN_H_
#define O3D_CORE_CROSS_CAIRO_PATTERN_H_

#include "core/cross/object_base.h"

typedef struct _cairo_pattern cairo_pattern_t;

namespace o3d {

class Pack;
class Texture;

namespace o2d {

class Pattern : public ObjectBase {
 public:
  typedef SmartPointer<Pattern> Ref;

  enum Filter {
    FAST,
    GOOD,
    BEST,
    NEAREST,
    BILINEAR
  };

  // Create a pattern that paints the content of a texture.
  static Pattern* CreateTexturePattern(Pack* pack, Texture* texture);

  // Create a pattern that paints a solid colour.
  static Pattern* CreateRgbPattern(Pack* pack,
                                   double red,
                                   double green,
                                   double blue);

  // Create a pattern that paints a solid colour with transparency.
  static Pattern* CreateRgbaPattern(Pack* pack,
                                    double red,
                                    double green,
                                    double blue,
                                    double alpha);

  virtual ~Pattern();

  cairo_pattern_t* pattern() const { return pattern_; }

  // Set the affine transformation matrix that maps user space to pattern space.
  // The default matrix is the identity matrix, so that no transformation
  // occurs.
  void SetAffineTransform(double xx,
                          double yx,
                          double xy,
                          double yy,
                          double x0,
                          double y0);

  void set_filter(Filter filter);

 private:
  Pattern(ServiceLocator* service_locator, cairo_pattern_t* pattern);

  static Pattern* WrapCairoPattern(Pack* pack, cairo_pattern_t* pattern);

  cairo_pattern_t* pattern_;

  O3D_DECL_CLASS(Pattern, ObjectBase)
};  // Pattern

}  // namespace o2d

}  // namespace o3d

#endif  // O3D_CORE_CROSS_CAIRO_PATTERN_H_
