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


#ifndef O3D_CORE_CROSS_TEXTURE_H_
#define O3D_CORE_CROSS_TEXTURE_H_

#include <vector>

#include "core/cross/render_surface.h"
#include "core/cross/texture_base.h"

namespace o3d {

class Pack;
class Bitmap;
class Canvas;

// An abstract class for 2D textures that defines the interface for getting
// the dimensions of the texture and number of mipmap levels.
// Concrete implementations should implement the Lock and Unlock methods.
class Texture2D : public Texture {
 public:
  typedef SmartPointer<Texture2D> Ref;

  // Class to help lock Texture2D. Automatically unlocks texture in destructor.
  class LockHelper {
   public:
    explicit LockHelper(Texture2D* texture, int level);
    ~LockHelper();

    int pitch() const {
      return pitch_;
    }

    // Gets a pointer to the data of the buffer, locking the buffer if
    // necessary.
    // Returns:
    //   Pointer to data in buffer or NULL if there was an error.
    void* GetData();

    // Typed version of GetData
    template <typename T>
    T* GetDataAs() {
      return reinterpret_cast<T*>(GetData());
    }

   private:
    Texture2D* texture_;
    int level_;
    int pitch_;
    void* data_;
    bool locked_;

    DISALLOW_COPY_AND_ASSIGN(LockHelper);
  };


  Texture2D(ServiceLocator* service_locator,
            int width,
            int height,
            Format format,
            int levels,
            bool enable_render_surfaces);
  virtual ~Texture2D();

  static const char* kWidthParamName;
  static const char* kHeightParamName;

  inline int width() const {
    return width_param_->value();
  }

  inline int height() const {
    return height_param_->value();
  }

  //   level: The mipmap level to modify
  //   dst_left: The left edge of the rectangular area to modify.
  //   dst_top: The top edge of the rectangular area to modify.
  //   width: The width of the rectangular area to modify.
  //   height: The of the rectangular area to modify.
  //   src_data: The source pixels.
  //   src_pitch: If the format is uncompressed this is the number of bytes
  //      per row of pixels. If compressed this value is unused.
  virtual void SetRect(int level,
                       unsigned dst_left,
                       unsigned dst_top,
                       unsigned width,
                       unsigned height,
                       const void* src_data,
                       int src_pitch) = 0;

  // Returns a RenderSurface object associated with a mip_level of a texture.
  // Parameters:
  //  mip_level: [in] The mip-level of the surface to be returned.
  //  pack: [in] The pack in which the surface will reside.
  // Returns:
  //  Reference to the RenderSurface object.
  virtual RenderSurface::Ref GetRenderSurface(int mip_level, Pack* pack) = 0;

  // Copy pixels from source bitmap to certain mip level.
  // Scales if the width and height of source and dest do not match.
  // Parameters:
  //   source_img: source bitmap which would be drawn.
  //   source_mip: source mip to draw.
  //   source_x: x-coordinate of the starting pixel in the source image.
  //   source_y: y-coordinate of the starting pixel in the source image.
  //   source_width: width of the source image to draw.
  //   source_height: Height of the source image to draw.
  //   dest_mip: on which mip level the sourceImg would be drawn.
  //   dest_x: x-coordinate of the starting pixel in the dest image.
  //   dest_y: y-coordinate of the starting pixel in the dest image.
  //   dest_width: width of the dest image.
  //   dest_height: height of the dest image.
  void DrawImage(const Bitmap& source_img, int src_mip,
                 int source_x, int source_y,
                 int source_width, int source_height,
                 int dest_mip,
                 int dest_x, int dest_y,
                 int dest_width, int dest_height);

  // Copy pixels from source bitmap to certain mip level.
  // Scales if the width and height of source and dest do not match.
  // Parameters:
  //   source_img: source canvas to draw.
  //   source_x: x-coordinate of the starting pixel in the source image.
  //   source_y: y-coordinate of the starting pixel in the source image.
  //   source_width: width of the source image to draw.
  //   source_height: Height of the source image to draw.
  //   dest_mip: the dest mip to draw to.
  //   dest_x: x-coordinate of the starting pixel in the dest image.
  //   dest_y: y-coordinate of the starting pixel in the dest image.
  //   dest_width: width of the dest image.
  //   dest_height: height of the dest image.
  void DrawImage(const Canvas& source_img,
                 int source_x, int source_y,
                 int source_width, int source_height,
                 int dest_mip,
                 int dest_x, int dest_y,
                 int dest_width, int dest_height);


  // Sets the contents of the texture from a Bitmap.
  void SetFromBitmap(const Bitmap& bitmap);

  // Overridden from Texture.
  virtual void GenerateMips(int source_level, int num_levels);

 protected:
  // Returns a pointer to the internal texture data for the given mipmap level.
  // Lock must be called before the texture data can be modified.
  // Parameters:
  //  level: [in] the mipmap level to be modified
  //  texture_data: [out] a pointer to the current texture data
  //  pitch: bytes across 1 row of pixels if uncompressed format. bytes across 1
  //     row of blocks if compressed format.
  // Returns:
  //  true if the operation succeeds
  virtual bool Lock(int level, void** texture_data, int* pitch) = 0;

  // Notifies the texture object that the internal texture data has been
  // been modified.  Unlock must be called in conjunction with Lock.  Modifying
  // the contents of the texture after Unlock has been called could lead to
  // unpredictable behavior.
  // Parameters:
  //  level: [in] the mipmap level that was modified
  // Returns:
  //  true if the operation succeeds
  virtual bool Unlock(int level) = 0;

  // Returns true if the mip-map level has been locked.
  bool IsLocked(unsigned int level) {
    DCHECK_LT(static_cast<int>(level), levels());
    return (locked_levels_ & (1 << level)) != 0;
  }

  // Bitfield that indicates mip levels that are currently locked.
  unsigned int locked_levels_;

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // The width of the texture, in texels.
  ParamInteger::Ref width_param_;
  // The height of the texture, in texels.
  ParamInteger::Ref height_param_;

  O3D_DECL_CLASS(Texture2D, Texture);
  DISALLOW_COPY_AND_ASSIGN(Texture2D);
};

class TextureCUBE : public Texture {
 public:
  typedef SmartPointer<TextureCUBE> Ref;
  // Cross-platform enumeration of faces of the cube texture.
  enum CubeFace {
    FACE_POSITIVE_X,
    FACE_NEGATIVE_X,
    FACE_POSITIVE_Y,
    FACE_NEGATIVE_Y,
    FACE_POSITIVE_Z,
    FACE_NEGATIVE_Z,
    NUMBER_OF_FACES,
  };

  // Class to help lock TextureCUBE.
  // Automatically unlocks texture in destructor.
  class LockHelper {
   public:
    explicit LockHelper(TextureCUBE* texture, CubeFace face, int level);
    ~LockHelper();

    int pitch() const {
      return pitch_;
    }

    // Gets a pointer to the data of the buffer, locking the buffer if
    // necessary.
    // Returns:
    //   Pointer to data in buffer or NULL if there was an error.
    void* GetData();

    // Typed version of GetData
    template <typename T>
    T* GetDataAs() {
      return reinterpret_cast<T*>(GetData());
    }

   private:
    TextureCUBE* texture_;
    CubeFace face_;
    int level_;
    int pitch_;
    void* data_;
    bool locked_;

    DISALLOW_COPY_AND_ASSIGN(LockHelper);
  };

  static const char* kEdgeLengthParamName;

  TextureCUBE(ServiceLocator* service_locator,
              int edge_length,
              Format format,
              int levels,
              bool enable_render_surfaces);

  virtual ~TextureCUBE();
  inline int edge_length() const {
    return edge_length_param_->value();
  }

  // Sets a rectangular region of this texture.
  // If the texture is a DXT format, the only acceptable values
  // for left, top, width and height are 0, 0, texture->width, texture->height
  //
  // Parameters:
  //   face: The face of the cube to modify.
  //   level: The mipmap level to modify
  //   dst_left: The left edge of the rectangular area to modify.
  //   dst_top: The top edge of the rectangular area to modify.
  //   width: The width of the rectangular area to modify.
  //   height: The of the rectangular area to modify.
  //   src_data: buffer to get pixels from.
  //   src_pitch: If the format is uncompressed this is the number of bytes
  //      per row of pixels. If compressed this value is unused.
  virtual void SetRect(CubeFace face,
                       int level,
                       unsigned dst_left,
                       unsigned dst_top,
                       unsigned width,
                       unsigned height,
                       const void* src_data,
                       int src_pitch) = 0;

  // Returns a RenderSurface object associated with a given cube face and
  // mip_level of a texture.
  // Parameters:
  //  face: [in] The cube face from which to extract the surface.
  //  mip_level: [in] The mip-level of the surface to be returned.
  //  pack: [in] The pack in which the surface will reside.
  // Returns:
  //  Reference to the RenderSurface object.
  virtual RenderSurface::Ref GetRenderSurface(CubeFace face,
                                              int level,
                                              Pack* pack) = 0;

  // Copy pixels from source bitmap to certain mip level.
  // Scales if the width and height of source and dest do not match.
  // Parameters:
  //   source_img: source bitmap.
  //   source_mip: source mip to draw.
  //   source_x: x-coordinate of the starting pixel in the source image.
  //   source_y: y-coordinate of the starting pixel in the source image.
  //   source_width: width of the source image to draw.
  //   source_height: Height of the source image to draw.
  //   face: face to draw to.
  //   dest_mip: mip to draw to.
  //   dest_x: x-coordinate of the starting pixel in the dest image.
  //   dest_y: y-coordinate of the starting pixel in the dest image.
  //   dest_width: width of the dest image.
  //   dest_height: height of the dest image.
  void DrawImage(const Bitmap& source_img, int source_mip,
                 int source_x, int source_y,
                 int source_width, int source_height,
                 CubeFace face, int dest_mip,
                 int dest_x, int dest_y, int dest_width,
                 int dest_height);

  // Copy pixels from source canvas to certain mip level.
  // Scales if the width and height of source and dest do not match.
  // Parameters:
  //   source_img: source canvas.
  //   source_x: x-coordinate of the starting pixel in the source image.
  //   source_y: y-coordinate of the starting pixel in the source image.
  //   source_width: width of the source image to draw.
  //   source_height: Height of the source image to draw.
  //   face: face to draw to.
  //   dest_mip: mip to draw to.
  //   dest_x: x-coordinate of the starting pixel in the dest image.
  //   dest_y: y-coordinate of the starting pixel in the dest image.
  //   dest_width: width of the dest image.
  //   dest_height: height of the dest image.
  void DrawImage(const Canvas& source_img,
                 int source_x, int source_y,
                 int source_width, int source_height,
                 CubeFace face, int dest_mip,
                 int dest_x, int dest_y, int dest_width,
                 int dest_height);

  // Sets the contents of the texture from a Bitmap.
  void SetFromBitmap(CubeFace face, const Bitmap& bitmap);

  // Overridden from Texture.
  virtual void GenerateMips(int source_level, int num_levels);

 protected:
  // Returns a pointer to the internal texture data for the given face and
  // mipmap level.
  // Lock must be called before the texture data can be modified.
  // Parameters:
  //  face: [in] the index of the cube face to be modified
  //  level: [in] the mipmap level to be modified
  //  texture_data: [out] a pointer to the current texture data
  //  pitch: bytes across 1 row of pixels if uncompressed format. bytes across 1
  //     row of blocks if compressed format.
  // Returns:
  //  true if the operation succeeds
  virtual bool Lock(
      CubeFace face, int level, void** texture_data, int* pitch) = 0;

  // Notifies the texture object that the internal texture data has been
  // been modified.  Unlock must be called in conjunction with Lock.
  // Modifying the contents of the texture after Unlock has been called could
  // lead to unpredictable behavior.
  // Parameters:
  //  face: [in] the index of the cube face that was modified
  //  level: [in] the mipmap level that was modified
  // Returns:
  //  true if the operation succeeds
  virtual bool Unlock(CubeFace face, int level) = 0;

  // Returns true if the mip-map level has been locked.
  bool IsLocked(unsigned int level, CubeFace face) {
    DCHECK_LT(static_cast<int>(level), levels());
    return (locked_levels_[face] & (1 << level)) != 0;
  }

  // Bitfields that indicates mip levels that are currently locked, one per
  // face.
  unsigned int locked_levels_[NUMBER_OF_FACES];

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // The length of each edge of the cube, in texels.
  ParamInteger::Ref edge_length_param_;

  O3D_DECL_CLASS(TextureCUBE, Texture);
  DISALLOW_COPY_AND_ASSIGN(TextureCUBE);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_TEXTURE_H_
