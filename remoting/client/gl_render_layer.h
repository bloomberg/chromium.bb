// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_OPENGL_GL_RENDER_LAYER_H_
#define REMOTING_CLIENT_OPENGL_GL_RENDER_LAYER_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "remoting/client/sys_opengl.h"

namespace remoting {
class GlCanvas;

// This class is for drawing a texture on the canvas. Must be deleted before the
// canvas is deleted.
class GlRenderLayer {
 public:
  // texture_id: An integer in range [0, GL_MAX_TEXTURE_IMAGE_UNITS], defining
  //             which slot to store the texture.
  GlRenderLayer(int texture_id, GlCanvas* canvas);
  ~GlRenderLayer();

  // Sets the texture (RGBA 8888) to be drawn. Please use UpdateTexture() if the
  // texture size isn't changed.
  void SetTexture(const uint8_t* texture, int width, int height);

  // Updates a subregion (RGBA 8888) of the texture.
  // stride: byte distance between two rows in |subtexture|.
  // If |stride| is 0 or |stride| == |width|*kBytesPerPixel, |subtexture| will
  // be treated as tightly packed.
  void UpdateTexture(const uint8_t* subtexture,
                     int offset_x,
                     int offset_y,
                     int width,
                     int height,
                     int stride);

  // Sets the positions of four vertices of the texture in normalized
  // coordinates. The default values are (0, 0), (0, 1), (1, 0), (1, 1),
  // i.e. stretching the texture to fit the whole canvas.
  // positions: [ x_upperleft, y_upperleft, x_lowerleft, y_lowerleft,
  //              x_upperright, y_upperright, x_lowerright, y_lowerright ]
  void SetVertexPositions(const std::array<float, 8>& positions);

  // Sets the visible area of the texture. The default values are (0, 0),
  // (0, 1), (1, 0), (1, 1), i.e. showing the whole texture.
  // positions: [ x_upperleft, y_upperleft, x_lowerleft, y_lowerleft,
  //              x_upperright, y_upperright, x_lowerright, y_lowerright ]
  void SetTextureVisibleArea(const std::array<float, 8>& positions);

  // Draws the texture on the canvas. Texture must be set before calling Draw().
  void Draw();

 private:
  int texture_id_;
  GlCanvas* canvas_;

  GLuint texture_handle_;
  GLuint buffer_handle_;

  // true IFF the texture is already set by calling SetTexture().
  bool texture_set_ = false;

  // Used in OpenGL ES 2 context which doesn't support GL_UNPACK_ROW_LENGTH to
  // tightly pack dirty regions before sending them to GPU.
  std::unique_ptr<uint8_t[]> update_buffer_;
  int update_buffer_size_ = 0;

  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(GlRenderLayer);
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_OPENGL_GL_RENDER_LAYER_H_
