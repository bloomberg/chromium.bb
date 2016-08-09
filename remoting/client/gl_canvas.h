// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_OPENGL_GL_CANVAS_H_
#define REMOTING_CLIENT_OPENGL_GL_CANVAS_H_

#include <array>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "remoting/client/sys_opengl.h"

namespace remoting {

// This class holds zoom and pan configurations of the canvas and is used to
// draw textures on the canvas.
// Must be constructed after the OpenGL surface is created and destroyed before
// the surface is destroyed.
class GlCanvas {
 public:
  // gl_version: version number of the OpenGL ES context. Either 2 or 3.
  GlCanvas(int gl_version);

  ~GlCanvas();

  // Sets the transformation matrix. This matrix defines how the canvas should
  // be shown on the view.
  // 3 by 3 transformation matrix, [ m0, m1, m2, m3, m4, m5, m6, m7, m8 ].
  // The matrix will be multiplied with the positions (with projective space,
  // (x, y, 1)) to draw the textures with the right zoom and pan configuration.
  //
  // | m0, m1, m2, |   | x |
  // | m3, m4, m5, | * | y |
  // | m6, m7, m8  |   | 1 |
  //
  // For a typical transformation matrix such that m1=m3=m6=m7=0 and m8=1, m0
  // and m4 defines the scaling factor of the canvas and m2 and m5 defines the
  // offset of the upper-left corner in pixel.
  void SetTransformationMatrix(const std::array<float, 9>& matrix);

  // Sets the size of the view in pixels.
  void SetViewSize(int width, int height);

  // Draws the texture on the canvas. Nothing will happen if
  // SetNormalizedTransformation() has not been called.
  // vertex_buffer: reference to the 2x4x2 float vertex buffer.
  //                [ four (x, y) position of the texture vertices in pixel
  //                              with respect to the canvas,
  //                  four (x, y) position of the vertices in percentage
  //                              defining the visible area of the texture ]
  // alpha_multiplier: Will be multiplied with the alpha channel of the texture.
  //                   Passing 1 means no change of the transparency of the
  //                   texture.
  void DrawTexture(int texture_id,
                   GLuint texture_handle,
                   GLuint vertex_buffer,
                   float alpha_multiplier);

  // Returns the version number of current OpenGL ES context. Either 2 or 3.
  int GetGlVersion() const;

  // Returns the maximum texture resolution limitation. Neither the width nor
  // the height of the texture can exceed this limitation.
  int GetMaxTextureSize() const;

 private:
  int gl_version_;

  int max_texture_size_ = 0;
  bool transformation_set_ = false;
  bool view_size_set_ = false;

  // Handles.
  GLuint vertex_shader_;
  GLuint fragment_shader_;
  GLuint program_;

  // Locations of the corresponding shader attributes.
  GLuint transform_location_;
  GLuint view_size_location_;
  GLuint texture_location_;
  GLuint alpha_multiplier_location_;
  GLuint position_location_;
  GLuint tex_cord_location_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(GlCanvas);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_OPENGL_GL_CANVAS_H_
