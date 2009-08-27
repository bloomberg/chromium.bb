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


#include "core/cross/precompile.h"
#include "core/win/d3d9/texture_d3d9.h"

#include "core/cross/error.h"
#include "core/cross/types.h"
#include "core/win/d3d9/utils_d3d9.h"
#include "core/win/d3d9/renderer_d3d9.h"
#include "core/win/d3d9/render_surface_d3d9.h"

namespace o3d {

namespace {

Texture::RGBASwizzleIndices g_d3d_abgr32f_swizzle_indices = {2, 1, 0, 3};

// Converts an O3D texture format to a D3D texture format.
D3DFORMAT DX9Format(Texture::Format format) {
  switch (format) {
    case Texture::XRGB8:  return D3DFMT_X8R8G8B8;
    case Texture::ARGB8:  return D3DFMT_A8R8G8B8;
    case Texture::ABGR16F:  return D3DFMT_A16B16G16R16F;
    case Texture::R32F:  return D3DFMT_R32F;
    case Texture::ABGR32F:  return D3DFMT_A32B32G32R32F;
    case Texture::DXT1:  return D3DFMT_DXT1;
    case Texture::DXT3:  return D3DFMT_DXT3;
    case Texture::DXT5:  return D3DFMT_DXT5;
    default:  return D3DFMT_UNKNOWN;
  };
}

// Converts a TextureCUBE::CubeFace value to an equivalent D3D9 value.
static D3DCUBEMAP_FACES DX9CubeFace(TextureCUBE::CubeFace face) {
  switch (face) {
    case TextureCUBE::FACE_POSITIVE_X:
      return D3DCUBEMAP_FACE_POSITIVE_X;
    case TextureCUBE::FACE_NEGATIVE_X:
      return D3DCUBEMAP_FACE_NEGATIVE_X;
    case TextureCUBE::FACE_POSITIVE_Y:
      return D3DCUBEMAP_FACE_POSITIVE_Y;
    case TextureCUBE::FACE_NEGATIVE_Y:
      return D3DCUBEMAP_FACE_NEGATIVE_Y;
    case TextureCUBE::FACE_POSITIVE_Z:
      return D3DCUBEMAP_FACE_POSITIVE_Z;
    case TextureCUBE::FACE_NEGATIVE_Z:
      return D3DCUBEMAP_FACE_NEGATIVE_Z;
  }

  // TODO: Figure out how to get errors out of here to the client.
  DLOG(ERROR) << "Unknown Cube Face enumeration " << face;
  return D3DCUBEMAP_FACE_FORCE_DWORD;
}

// Constructs an Direct3D texture object.  Out variable returns the status of
// the constructed texture including if resize to POT is required, and the
// actual mip dimensions used.
HRESULT CreateTexture2DD3D9(RendererD3D9* renderer,
                            Texture::Format format,
                            int levels,
                            int width,
                            int height,
                            bool enable_render_surfaces,
                            bool* resize_to_pot,
                            unsigned int* mip_width,
                            unsigned int* mip_height,
                            IDirect3DTexture9** d3d_texture) {
  IDirect3DDevice9 *d3d_device = renderer->d3d_device();
  *resize_to_pot = !renderer->supports_npot() && !image::IsPOT(width, height);
  *mip_width = width;
  *mip_height = height;

  if (*resize_to_pot) {
    *mip_width = image::ComputePOTSize(*mip_width);
    *mip_height = image::ComputePOTSize(*mip_height);
  }

  DWORD usage = (enable_render_surfaces) ? D3DUSAGE_RENDERTARGET : 0;
  D3DPOOL pool = (enable_render_surfaces) ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
  D3DFORMAT d3d_format = DX9Format(format);

  HRESULT tex_result = d3d_device->CreateTexture(*mip_width,
                                                 *mip_height,
                                                 levels,
                                                 usage,
                                                 d3d_format,
                                                 pool,
                                                 d3d_texture,
                                                 NULL);
  if (!HR(tex_result)) {
    DLOG(ERROR) << "2D texture creation failed with the following parameters: "
                << "(" << *mip_width << " x " << *mip_height << ") x "
                << levels << "; format = " << format;
  }
  return tex_result;
}

// Constructs an Direct3D cube texture object.  Out variable returns the
// status of the constructed texture including if resize to POT is required,
// and the actual mip edge length used.
HRESULT CreateTextureCUBED3D9(RendererD3D9* renderer,
                              int edge_length,
                              Texture::Format format,
                              int levels,
                              bool enable_render_surfaces,
                              bool* resize_to_pot,
                              unsigned int* edge_width,
                              IDirect3DCubeTexture9** d3d_texture) {
  IDirect3DDevice9 *d3d_device = renderer->d3d_device();
  *resize_to_pot = !renderer->supports_npot() &&
                   !image::IsPOT(edge_length, edge_length);
  *edge_width = edge_length;
  if (*resize_to_pot) {
    *edge_width = image::ComputePOTSize(*edge_width);
  }

  DWORD usage = (enable_render_surfaces) ? D3DUSAGE_RENDERTARGET : 0;
  D3DPOOL pool = (enable_render_surfaces) ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
  D3DFORMAT d3d_format = DX9Format(format);

  HRESULT tex_result = d3d_device->CreateCubeTexture(*edge_width,
                                                     levels,
                                                     usage,
                                                     d3d_format,
                                                     pool,
                                                     d3d_texture,
                                                     NULL);
  if (!HR(tex_result)) {
    DLOG(ERROR) << "CUBE texture creation failed with the following "
                << "parameters: "
                << "(" << *edge_width << " x " << *edge_width << ") x "
                << levels << "; format = " << format;
  }

  return tex_result;
}

// Class providing a construction callback routine for extracting a
// RenderSurface from a cube-face and mip-level of a cube-texture.
// Note:  This class maintains a reference-counted pointer to the texture
// object, so that the lifetime of the Texture is guaranteed to be at least
// as long as that of the class.
class CubeFaceSurfaceConstructor : public SurfaceConstructor {
 public:
  CubeFaceSurfaceConstructor(TextureCUBED3D9 *texture,
                             TextureCUBE::CubeFace face,
                             int mip_level)
      : cube_texture_(texture->GetWeakPointer()),
        face_(face),
        mip_level_(mip_level) {
  }

  virtual HRESULT ConstructSurface(IDirect3DSurface9** surface) {
    TextureCUBED3D9* texture =
        down_cast<TextureCUBED3D9*>(cube_texture_.Get());
    if (!texture) {
      return E_FAIL;
    }
    IDirect3DCubeTexture9* d3d_cube_texture =
        static_cast<IDirect3DCubeTexture9*>(texture->GetTextureHandle());
    return d3d_cube_texture->GetCubeMapSurface(DX9CubeFace(face_),
                                               mip_level_,
                                               surface);
  }

 private:
  Texture::WeakPointerType cube_texture_;
  TextureCUBE::CubeFace face_;
  int mip_level_;
  DISALLOW_COPY_AND_ASSIGN(CubeFaceSurfaceConstructor);
};

// Class providing a construction callback routine for extracting a
// RenderSurface from a mip-level of a texture.
// Note:  This class maintains a reference-counted pointer to the texture
// object, so that the lifetime of the Texture is guaranteed to be at least
// as long as that of the class.
class TextureSurfaceConstructor : public SurfaceConstructor {
 public:
  TextureSurfaceConstructor(Texture2DD3D9* texture, int mip_level)
      : texture_(texture->GetWeakPointer()),
        mip_level_(mip_level) {
  }

  virtual HRESULT ConstructSurface(IDirect3DSurface9** surface) {
    Texture2DD3D9* texture = down_cast<Texture2DD3D9*>(texture_.Get());
    if (!texture) {
      return E_FAIL;
    }
    IDirect3DTexture9* d3d_texture =
        static_cast<IDirect3DTexture9*>(texture->GetTextureHandle());
    return d3d_texture->GetSurfaceLevel(mip_level_, surface);
  }

 private:
  Texture::WeakPointerType texture_;
  int mip_level_;
  DISALLOW_COPY_AND_ASSIGN(TextureSurfaceConstructor);
};

void SetTextureRectUncompressed(Texture::Format format,
                                const uint8* src,
                                int src_pitch,
                                unsigned src_width,
                                unsigned src_height,
                                uint8* dst,
                                int dst_pitch) {
  size_t bytes_per_line = image::ComputePitch(format, src_width);
  for (unsigned yy = 0; yy < src_height; ++yy) {
    memcpy(dst, src, bytes_per_line);
    src += src_pitch;
    dst += dst_pitch;
  }
}

void SetTextureRectCompressed(Texture::Format format,
                              const uint8* src,
                              unsigned src_width,
                              unsigned src_height,
                              uint8* dst,
                              int dst_pitch) {
  unsigned blocks_across = (src_width + 3) / 4;
  unsigned blocks_down = (src_height + 3) / 4;
  unsigned bytes_per_block = format == Texture::DXT1 ? 8 : 16;
  unsigned bytes_per_row = bytes_per_block * blocks_across;
  for (unsigned yy = 0; yy < blocks_down; ++yy) {
    memcpy(dst, src, bytes_per_row);
    src += bytes_per_row;
    dst += dst_pitch;
  }
}

void SetTextureRect(
    ServiceLocator* service_locator,
    IDirect3DTexture9* d3d_texture,
    Texture::Format format,
    int level,
    unsigned dst_left,
    unsigned dst_top,
    unsigned src_width,
    unsigned src_height,
    const void* src_data,
    int src_pitch) {
  DCHECK(src_data);
  bool compressed = Texture::IsCompressedFormat(format);

  RECT rect = {dst_left, dst_top, dst_left + src_width, dst_top + src_height};
  D3DLOCKED_RECT out_rect = {0};

  if (!HR(d3d_texture->LockRect(
      level, &out_rect, compressed ? NULL : &rect, 0))) {
    O3D_ERROR(service_locator) << "Failed to Lock Texture2D (D3D9)";
    return;
  }

  const uint8* src = static_cast<const uint8*>(src_data);
  uint8* dst = static_cast<uint8*>(out_rect.pBits);
  if (!compressed) {
    SetTextureRectUncompressed(format, src, src_pitch, src_width, src_height,
                               dst, out_rect.Pitch);
  } else {
    SetTextureRectCompressed(
        format, src, src_width, src_height, dst, out_rect.Pitch);
  }
  if (!HR(d3d_texture->UnlockRect(level))) {
    O3D_ERROR(service_locator) << "Failed to Unlock Texture2D (D3D9)";
    return;
  }
}

void SetTextureFaceRect(
    ServiceLocator* service_locator,
    IDirect3DCubeTexture9* d3d_texture,
    Texture::Format format,
    TextureCUBE::CubeFace face,
    int level,
    unsigned dst_left,
    unsigned dst_top,
    unsigned src_width,
    unsigned src_height,
    const void* src_data,
    int src_pitch) {
  DCHECK(src_data);
  bool compressed = Texture::IsCompressedFormat(format);

  RECT rect = {dst_left, dst_top, dst_left + src_width, dst_top + src_height};
  D3DLOCKED_RECT out_rect = {0};

  D3DCUBEMAP_FACES d3d_face = DX9CubeFace(face);

  if (!HR(d3d_texture->LockRect(
      d3d_face, level, &out_rect, compressed ? NULL : &rect, 0))) {
    O3D_ERROR(service_locator) << "Failed to Lock TextureCUBE (D3D9)";
    return;
  }

  const uint8* src = static_cast<const uint8*>(src_data);
  uint8* dst = static_cast<uint8*>(out_rect.pBits);
  if (!compressed) {
    SetTextureRectUncompressed(format, src, src_pitch, src_width, src_height,
                               dst, out_rect.Pitch);
  } else {
    SetTextureRectCompressed(
        format, src, src_width, src_height, dst, out_rect.Pitch);
  }
  if (!HR(d3d_texture->UnlockRect(d3d_face, level))) {
    O3D_ERROR(service_locator) << "Failed to Unlock TextureCUBE (D3D9)";
    return;
  }
}

}  // unnamed namespace

// Constructs a 2D texture object from the given (existing) D3D 2D texture.
Texture2DD3D9::Texture2DD3D9(ServiceLocator* service_locator,
                             IDirect3DTexture9* tex,
                             Texture::Format format,
                             int levels,
                             int width,
                             int height,
                             bool resize_to_pot,
                             bool enable_render_surfaces)
    : Texture2D(service_locator,
                width,
                height,
                format,
                levels,
                enable_render_surfaces),
      resize_to_pot_(resize_to_pot),
      d3d_texture_(tex),
      backing_bitmap_(Bitmap::Ref(new Bitmap(service_locator))) {
  DCHECK(tex);
}

// Attempts to create a IDirect3DTexture9 with the given specs.  If the creation
// of the texture succeeds then it creates a Texture2DD3D9 object around it and
// returns it.  This is the safe way to create a Texture2DD3D9 object that
// contains a valid D3D9 texture.
Texture2DD3D9* Texture2DD3D9::Create(ServiceLocator* service_locator,
                                     Texture::Format format,
                                     int levels,
                                     int width,
                                     int height,
                                     RendererD3D9* renderer,
                                     bool enable_render_surfaces) {
  DCHECK_NE(format, Texture::UNKNOWN_FORMAT);
  CComPtr<IDirect3DTexture9> d3d_texture;
  bool resize_to_pot;
  unsigned int mip_width, mip_height;
  if (!HR(CreateTexture2DD3D9(renderer,
                              format,
                              levels,
                              width,
                              height,
                              enable_render_surfaces,
                              &resize_to_pot,
                              &mip_width,
                              &mip_height,
                              &d3d_texture))) {
    DLOG(ERROR) << "Failed to create Texture2D (D3D9) : ";
    return NULL;
  }

  Texture2DD3D9 *texture = new Texture2DD3D9(service_locator,
                                             d3d_texture,
                                             format,
                                             levels,
                                             width,
                                             height,
                                             resize_to_pot,
                                             enable_render_surfaces);

  // Clear the texture.
  // This is platform specific because some platforms, (command-buffers),
  // will guarantee the textures are cleared so we don't have to.
  {
    size_t row_size = image::ComputeMipChainSize(mip_width, 1, format, 1);
    scoped_array<uint8> zero(new uint8[row_size]);
    memset(zero.get(), 0, row_size);
    for (int level = 0; level < levels; ++level) {
      if (enable_render_surfaces) {
        texture->GetRenderSurface(level);
      } else {
        texture->SetRect(level, 0, 0, 
                         image::ComputeMipDimension(level, mip_width),
                         image::ComputeMipDimension(level, mip_height),
                         zero.get(), 0);
      }
    }
  }

  if (resize_to_pot) {
    texture->backing_bitmap_->Allocate(format, width, height, levels,
                                       Bitmap::IMAGE);
  }

  return texture;
}

// Destructor releases the D3D9 texture resource.
Texture2DD3D9::~Texture2DD3D9() {
  d3d_texture_ = NULL;
}

void Texture2DD3D9::UpdateBackedMipLevel(unsigned int level) {
  DCHECK_LT(level, static_cast<unsigned int>(levels()));
  DCHECK(backing_bitmap_->image_data());
  DCHECK_EQ(backing_bitmap_->width(), width());
  DCHECK_EQ(backing_bitmap_->height(), height());
  DCHECK_EQ(backing_bitmap_->format(), format());
  DCHECK_EQ(backing_bitmap_->num_mipmaps(), levels());

  unsigned int mip_width = image::ComputeMipDimension(level, width());
  unsigned int mip_height = image::ComputeMipDimension(level, height());
  unsigned int rect_width = mip_width;
  unsigned int rect_height = mip_height;
  rect_width = std::max(1U, image::ComputePOTSize(width()) >> level);
  rect_height = std::max(1U, image::ComputePOTSize(height()) >> level);

  RECT rect = {0, 0, rect_width, rect_height};
  D3DLOCKED_RECT out_rect = {0};

  if (!HR(d3d_texture_->LockRect(level, &out_rect, &rect, 0))) {
    O3D_ERROR(service_locator())
        << "Failed to lock texture level " << level << ".";
    return;
  }

  DCHECK(out_rect.pBits);
  uint8* dst = static_cast<uint8*>(out_rect.pBits);

  const uint8 *mip_data = backing_bitmap_->GetMipData(level);
  if (resize_to_pot_) {
    image::Scale(mip_width, mip_height, format(), mip_data,
                 rect_width, rect_height,
                 static_cast<uint8 *>(out_rect.pBits),
                 out_rect.Pitch);
  } else {
    if (!IsCompressed()) {
      SetTextureRectUncompressed(
          format(), mip_data,
          image::ComputePitch(format(), mip_width),
          mip_width, mip_height,
          dst, out_rect.Pitch);
    } else {
      SetTextureRectCompressed(
          format(), mip_data, mip_width, mip_height, dst, out_rect.Pitch);
    }
  }

  if (!HR(d3d_texture_->UnlockRect(level))) {
    O3D_ERROR(service_locator())
        << "Failed to unlock texture level " << level << ".";
  }
}

RenderSurface::Ref Texture2DD3D9::PlatformSpecificGetRenderSurface(
    int mip_level) {
  if (!render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to get RenderSurface from non-render-surface-enabled"
        << " Texture: " << name();
    return RenderSurface::Ref(NULL);
  }

  if (mip_level >= levels() || mip_level < 0) {
    O3D_ERROR(service_locator())
        << "Attempting to access non-existent mip_level " << mip_level
        << " in render-target texture \"" << name() << "\".";
    return RenderSurface::Ref(NULL);
  }

  return RenderSurface::Ref(
      new RenderSurfaceD3D9(
          service_locator(),
          width() >> mip_level,
          height() >> mip_level,
          this,
          new TextureSurfaceConstructor(this, mip_level)));
}

void Texture2DD3D9::SetRect(int level,
                            unsigned dst_left,
                            unsigned dst_top,
                            unsigned src_width,
                            unsigned src_height,
                            const void* src_data,
                            int src_pitch) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to SetRect on non-existent level " << level
        << " on Texture \"" << name() << "\"";
    return;
  }
  if (render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to SetRect a render-target texture: " << name();
    return;
  }

  unsigned mip_width = image::ComputeMipDimension(level, width());
  unsigned mip_height = image::ComputeMipDimension(level, height());

  if (dst_left + src_width > mip_width ||
      dst_top + src_height > mip_height) {
    O3D_ERROR(service_locator())
        << "SetRect(" << level << ", " << dst_left << ", " << dst_top << ", "
        << src_width << ", " << src_height << ") out of range for texture << \""
        << name() << "\"";
    return;
  }

  bool entire_rect = dst_left == 0 && dst_top == 0 &&
                     src_width == mip_width && src_height == mip_height;
  bool compressed = IsCompressed();

  if (compressed && !entire_rect) {
    O3D_ERROR(service_locator())
        << "SetRect must be full rectangle for compressed textures";
    return;
  }

  if (resize_to_pot_) {
    DCHECK(backing_bitmap_->image_data());
    DCHECK(!compressed);
    // We need to update the backing mipmap and then use that to update the
    // texture.
    backing_bitmap_->SetRect(
        level, dst_left, dst_top, src_width, src_height, src_data, src_pitch);
    UpdateBackedMipLevel(level);
  } else {
    SetTextureRect(service_locator(),
                   d3d_texture_,
                   format(),
                   level,
                   dst_left,
                   dst_top,
                   src_width,
                   src_height,
                   src_data,
                   src_pitch);
  }
}

// Locks the given mipmap level of this texture for loading from main memory,
// and returns a pointer to the buffer.
bool Texture2DD3D9::Lock(int level, void** texture_data, int* pitch) {
  DCHECK(texture_data);
  DCHECK(pitch);
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to lock inexistent level " << level << " on Texture \""
        << name() << "\"";
    return false;
  }
  if (IsLocked(level)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " of texture \"" << name()
        << "\" is already locked.";
    return false;
  }
  if (render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to lock a render-target texture: " << name();
    return false;
  }

  unsigned int mip_width = image::ComputeMipDimension(level, width());
  unsigned int mip_height = image::ComputeMipDimension(level, height());

  if (resize_to_pot_) {
    DCHECK(backing_bitmap_->image_data());
    *texture_data = backing_bitmap_->GetMipData(level);
    *pitch = image::ComputePitch(format(), mip_width);
    locked_levels_ |= 1 << level;
    return true;
  } else {
    RECT rect = {0, 0, mip_width, mip_height};
    D3DLOCKED_RECT out_rect = {0};

    if (HR(d3d_texture_->LockRect(level, &out_rect, &rect, 0))) {
      *texture_data = out_rect.pBits;
      *pitch = out_rect.Pitch;
      locked_levels_ |= 1 << level;
      return true;
    } else {
      O3D_ERROR(service_locator()) << "Failed to Lock Texture2D (D3D9)";
      *texture_data = NULL;
      return false;
    }
  }
}

// Unlocks the given mipmap level of this texture.
bool Texture2DD3D9::Unlock(int level) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to unlock inexistent level " << level << " on Texture \""
        << name() << "\"";
    return false;
  }
  if (!IsLocked(level)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " of texture \"" << name()
        << "\" is not locked.";
    return false;
  }
  bool result = false;
  if (resize_to_pot_) {
    UpdateBackedMipLevel(level);
    result = true;
  } else {
    result = HR(d3d_texture_->UnlockRect(level));
  }
  if (result) {
    locked_levels_ &= ~(1 << level);
  } else {
    O3D_ERROR(service_locator()) << "Failed to Unlock Texture2D (D3D9)";
  }
  return result;
}

bool Texture2DD3D9::OnLostDevice() {
  // Textures created with RenderSurface support are placed in the default
  // pool, so release them here.
  if (render_surfaces_enabled()) {
    d3d_texture_ = NULL;
  }
  return true;
}

bool Texture2DD3D9::OnResetDevice() {
  if (render_surfaces_enabled()) {
    DCHECK(d3d_texture_ == NULL);
    Renderer* renderer = service_locator()->GetService<Renderer>();
    RendererD3D9 *renderer_d3d9 = down_cast<RendererD3D9*>(renderer);
    bool resize_to_pot;
    unsigned int mip_width, mip_height;
    return HR(CreateTexture2DD3D9(renderer_d3d9,
                                  format(),
                                  levels(),
                                  width(),
                                  height(),
                                  render_surfaces_enabled(),
                                  &resize_to_pot,
                                  &mip_width,
                                  &mip_height,
                                  &d3d_texture_));
  }
  return true;
}

const Texture::RGBASwizzleIndices& Texture2DD3D9::GetABGR32FSwizzleIndices() {
  return g_d3d_abgr32f_swizzle_indices;
}

// Constructs a cube texture object from the given (existing) D3D Cube texture.
TextureCUBED3D9::TextureCUBED3D9(ServiceLocator* service_locator,
                                 IDirect3DCubeTexture9* tex,
                                 int edge_length,
                                 Texture::Format format,
                                 int levels,
                                 bool resize_to_pot,
                                 bool enable_render_surfaces)
    : TextureCUBE(service_locator,
                  edge_length,
                  format,
                  levels,
                  enable_render_surfaces),
      resize_to_pot_(resize_to_pot),
      d3d_cube_texture_(tex) {
  for (int ii = 0; ii < static_cast<int>(NUMBER_OF_FACES); ++ii) {
    backing_bitmaps_[ii] = Bitmap::Ref(new Bitmap(service_locator));
  }
}

// Attempts to create a D3D9 CubeTexture with the given specs.  If creation
// fails the method returns NULL.  Otherwise, it wraps around the newly created
// texture a TextureCUBED3D9 object and returns a pointer to it.
TextureCUBED3D9* TextureCUBED3D9::Create(ServiceLocator* service_locator,
                                         Texture::Format format,
                                         int levels,
                                         int edge_length,
                                         RendererD3D9 *renderer,
                                         bool enable_render_surfaces) {
  DCHECK_NE(format, Texture::UNKNOWN_FORMAT);
  DCHECK_GE(levels, 1);

  CComPtr<IDirect3DCubeTexture9> d3d_texture;
  bool resize_to_pot;
  unsigned int edge;
  if (!HR(CreateTextureCUBED3D9(renderer,
                                edge_length,
                                format,
                                levels,
                                enable_render_surfaces,
                                &resize_to_pot,
                                &edge,
                                &d3d_texture))) {
    DLOG(ERROR) << "Failed to create TextureCUBE (D3D9)";
    return NULL;
  }

  TextureCUBED3D9 *texture = new TextureCUBED3D9(service_locator,
                                                 d3d_texture,
                                                 edge_length,
                                                 format,
                                                 levels,
                                                 resize_to_pot,
                                                 enable_render_surfaces);

  // Clear the texture.
  // This is platform specific because some platforms, (command-buffers), will
  // guarantee the textures are cleared so we don't have to.
  {
    size_t row_size = image::ComputeMipChainSize(edge, 1, format, 1);
    scoped_array<uint8> zero(new uint8[row_size]);
    memset(zero.get(), 0, row_size);
    for (int level = 0; level < levels; ++level) {
      for (int face = 0; face < static_cast<int>(NUMBER_OF_FACES); ++face) {
        if (enable_render_surfaces) {
          texture->GetRenderSurface(static_cast<CubeFace>(face), level);
        } else {
          texture->SetRect(static_cast<CubeFace>(face),level , 0, 0, 
                           image::ComputeMipDimension(level, edge),
                           image::ComputeMipDimension(level, edge),
                           zero.get(), 0);
        }
      }
    }
  }
  if (resize_to_pot) {
    for (int ii = 0; ii < static_cast<int>(NUMBER_OF_FACES); ++ii) {
      texture->backing_bitmaps_[ii]->Allocate(
          format, edge_length, edge_length, levels, Bitmap::IMAGE);
    }
  }

  return texture;
}



// Destructor releases the D3D9 texture resource.
TextureCUBED3D9::~TextureCUBED3D9() {
  for (unsigned int i = 0; i < 6; ++i) {
    if (locked_levels_[i] != 0) {
      O3D_ERROR(service_locator())
          << "TextureCUBE \"" << name() << "\" was never unlocked before "
          << "being destroyed.";
      break;  // No need to report it more than once.
    }
  }
  d3d_cube_texture_ = NULL;
}

void TextureCUBED3D9::UpdateBackedMipLevel(TextureCUBE::CubeFace face,
                                           unsigned int level) {
  Bitmap* backing_bitmap = backing_bitmaps_[face].Get();
  DCHECK_LT(level, static_cast<unsigned int>(levels()));
  DCHECK(backing_bitmap->image_data());
  DCHECK_EQ(backing_bitmap->width(), edge_length());
  DCHECK_EQ(backing_bitmap->height(), edge_length());
  DCHECK_EQ(backing_bitmap->format(), format());
  DCHECK_EQ(backing_bitmap->num_mipmaps(), levels());

  unsigned int mip_edge = std::max(1, edge_length() >> level);
  unsigned int rect_edge = mip_edge;
  if (resize_to_pot_) {
    rect_edge = std::max(1U, image::ComputePOTSize(edge_length()) >> level);
  }

  RECT rect = {0, 0, rect_edge, rect_edge};
  D3DLOCKED_RECT out_rect = {0};
  D3DCUBEMAP_FACES d3d_face = DX9CubeFace(face);

  if (!HR(d3d_cube_texture_->LockRect(d3d_face, level, &out_rect, &rect, 0))) {
    O3D_ERROR(service_locator())
        << "Failed to lock texture level " << level << " face " << face << ".";
    return;
  }

  DCHECK(out_rect.pBits);
  uint8* dst = static_cast<uint8*>(out_rect.pBits);

  const uint8 *mip_data = backing_bitmap->GetMipData(level);
  if (resize_to_pot_) {
    image::Scale(mip_edge, mip_edge, format(), mip_data,
                 rect_edge, rect_edge,
                 static_cast<uint8 *>(out_rect.pBits),
                 out_rect.Pitch);
  } else {
    if (!IsCompressed()) {
      SetTextureRectUncompressed(
          format(), mip_data,
          image::ComputePitch(format(), mip_edge),
          mip_edge, mip_edge,
          dst, out_rect.Pitch);
    } else {
      SetTextureRectCompressed(
          format(), mip_data, mip_edge, mip_edge, dst, out_rect.Pitch);
    }
  }

  if (!HR(d3d_cube_texture_->UnlockRect(d3d_face, level))) {
    O3D_ERROR(service_locator())
        << "Failed to unlock texture level " << level << " face " << face
        << ".";
  }
}

RenderSurface::Ref TextureCUBED3D9::PlatformSpecificGetRenderSurface(
    TextureCUBE::CubeFace face,
    int mip_level) {
  if (!render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to get RenderSurface from non-render-surface-enabled"
        << " Texture: " << name();
    return RenderSurface::Ref(NULL);
  }

  if (mip_level >= levels() || mip_level < 0) {
    O3D_ERROR(service_locator())
        << "Attempting to access non-existent mip_level " << mip_level
        << " in render-target texture \"" << name() << "\".";
    return RenderSurface::Ref(NULL);
  }

  int edge = edge_length() >> mip_level;
  return RenderSurface::Ref(
      new RenderSurfaceD3D9(
          service_locator(),
          edge,
          edge,
          this,
          new CubeFaceSurfaceConstructor(this, face, mip_level)));
}

void TextureCUBED3D9::SetRect(TextureCUBE::CubeFace face,
                              int level,
                              unsigned dst_left,
                              unsigned dst_top,
                              unsigned src_width,
                              unsigned src_height,
                              const void* src_data,
                              int src_pitch) {
  if (static_cast<int>(face) < 0 || static_cast<int>(face) >= NUMBER_OF_FACES) {
    O3D_ERROR(service_locator())
        << "Trying to SetRect invalid face " << face << " on Texture \""
        << name() << "\"";
    return;
  }
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to SetRect non-existent level " << level
        << " on Texture \"" << name() << "\"";
    return;
  }
  if (render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to SetRect a render-target texture: " << name();
    return;
  }

  unsigned mip_width = image::ComputeMipDimension(level, edge_length());
  unsigned mip_height = mip_width;

  if (dst_left + src_width > mip_width ||
      dst_top + src_height > mip_height) {
    O3D_ERROR(service_locator())
        << "SetRect(" << level << ", " << dst_left << ", " << dst_top << ", "
        << src_width << ", " << src_height << ") out of range for texture << \""
        << name() << "\"";
    return;
  }

  bool entire_rect = dst_left == 0 && dst_top == 0 &&
                     src_width == mip_width && src_height == mip_height;
  bool compressed = IsCompressed();

  if (compressed && !entire_rect) {
    O3D_ERROR(service_locator())
        << "SetRect must be full rectangle for compressed textures";
    return;
  }

  if (resize_to_pot_) {
    Bitmap* backing_bitmap = backing_bitmaps_[face].Get();
    DCHECK(backing_bitmap->image_data());
    DCHECK(!compressed);
    // We need to update the backing mipmap and then use that to update the
    // texture.
    backing_bitmap->SetRect(
        level, dst_left, dst_top, src_width, src_height, src_data, src_pitch);
    UpdateBackedMipLevel(face, level);
  } else {
    SetTextureFaceRect(service_locator(),
                       d3d_cube_texture_,
                       format(),
                       face,
                       level,
                       dst_left,
                       dst_top,
                       src_width,
                       src_height,
                       src_data,
                       src_pitch);
  }
}

// Locks the given face and mipmap level of this texture for loading from
// main memory, and returns a pointer to the buffer.
bool TextureCUBED3D9::Lock(
    CubeFace face, int level, void** texture_data, int* pitch) {
  DCHECK(texture_data);
  DCHECK(pitch);
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to lock inexistent level " << level << " on Texture \""
        << name();
    return false;
  }
  if (IsLocked(level, face)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " Face " << face << " of texture \"" << name()
        << "\" is already locked.";
    return false;
  }
  if (render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to lock a render-target texture: " << name();
    return false;
  }

  unsigned int mip_width = image::ComputeMipDimension(level, edge_length());
  unsigned int mip_height = mip_width;

  if (resize_to_pot_) {
    Bitmap* backing_bitmap = backing_bitmaps_[face].Get();
    DCHECK(backing_bitmap->image_data());
    *texture_data = backing_bitmap->GetMipData(level);
    *pitch = image::ComputePitch(format(), mip_width);
    locked_levels_[face] |= 1 << level;
    return true;
  } else {
    RECT rect = {0, 0, mip_width, mip_height};
    D3DLOCKED_RECT out_rect = {0};

    if (HR(d3d_cube_texture_->LockRect(DX9CubeFace(face), level,
                                       &out_rect, &rect, 0))) {
      *texture_data = out_rect.pBits;
      *pitch = out_rect.Pitch;
      locked_levels_[face] |= 1 << level;
      return true;
    } else {
      O3D_ERROR(service_locator()) << "Failed to Lock Texture2D (D3D9)";
      *texture_data = NULL;
      return false;
    }
  }
}

// Unlocks the given face and mipmap level of this texture.
bool TextureCUBED3D9::Unlock(CubeFace face, int level) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to unlock inexistent level " << level << " on Texture \""
        << name();
    return false;
  }
  if (!IsLocked(level, face)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " of texture \"" << name()
        << "\" is not locked.";
    return false;
  }
  bool result = false;
  if (resize_to_pot_) {
    UpdateBackedMipLevel(face, level);
    result = true;
  } else {
    result = HR(d3d_cube_texture_->UnlockRect(DX9CubeFace(face),
                                              level));
  }
  if (result) {
    locked_levels_[face] &= ~(1 << level);
  } else {
    O3D_ERROR(service_locator()) << "Failed to Unlock Texture2D (D3D9)";
  }
  return result;
}

bool TextureCUBED3D9::OnLostDevice() {
  // Textures created with RenderSurface support are placed in the default
  // pool, so release them here.
  if (render_surfaces_enabled()) {
    d3d_cube_texture_ = NULL;
  }
  return true;
}

bool TextureCUBED3D9::OnResetDevice() {
  if (render_surfaces_enabled()) {
    DCHECK(d3d_cube_texture_ == NULL);
    Renderer* renderer = service_locator()->GetService<Renderer>();
    RendererD3D9 *renderer_d3d9 = down_cast<RendererD3D9*>(renderer);
    bool resize_to_pot;
    unsigned int mip_edge;
    return HR(CreateTextureCUBED3D9(renderer_d3d9,
                                    edge_length(),
                                    format(),
                                    levels(),
                                    render_surfaces_enabled(),
                                    &resize_to_pot,
                                    &mip_edge,
                                    &d3d_cube_texture_));
  }
  return true;
}

const Texture::RGBASwizzleIndices& TextureCUBED3D9::GetABGR32FSwizzleIndices() {
  return g_d3d_abgr32f_swizzle_indices;
}

}  // namespace o3d
