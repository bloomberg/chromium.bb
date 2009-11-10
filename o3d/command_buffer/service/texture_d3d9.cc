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


// This file implements the D3D9 versions of the texture resources, as well as
// the related GAPID3D9 function implementations.

#include "command_buffer/service/gapi_d3d9.h"
#include "command_buffer/service/texture_d3d9.h"

namespace command_buffer {
namespace o3d {

// Converts a texture format to a D3D texture format.
D3DFORMAT TextureD3D9::D3DFormat(texture::Format format) {
  switch (format) {
    case texture::kXRGB8:  return D3DFMT_X8R8G8B8;
    case texture::kARGB8:  return D3DFMT_A8R8G8B8;
    case texture::kABGR16F:  return D3DFMT_A16B16G16R16F;
    case texture::kR32F:  return D3DFMT_R32F;
    case texture::kABGR32F:  return D3DFMT_A32B32G32R32F;
    case texture::kDXT1:  return D3DFMT_DXT1;
    // TODO(petersont): Add DXT3/5 support.
    default:  return D3DFMT_UNKNOWN;
  };
}

// Converts a cube map face to a D3D face.
D3DCUBEMAP_FACES TextureD3D9::D3DFace(texture::Face face) {
  switch (face) {
    case texture::kFacePositiveX:
      return D3DCUBEMAP_FACE_POSITIVE_X;
    case texture::kFaceNegativeX:
      return D3DCUBEMAP_FACE_NEGATIVE_X;
    case texture::kFacePositiveY:
      return D3DCUBEMAP_FACE_POSITIVE_Y;
    case texture::kFaceNegativeY:
      return D3DCUBEMAP_FACE_NEGATIVE_Y;
    case texture::kFacePositiveZ:
      return D3DCUBEMAP_FACE_POSITIVE_Z;
    case texture::kFaceNegativeZ:
      return D3DCUBEMAP_FACE_NEGATIVE_Z;
  }
  LOG(FATAL) << "Not reached.";
  return D3DCUBEMAP_FACE_POSITIVE_X;
}

// Texture 2D functions

// Destroys the 2D texture, releasing the D3D texture, and its shadow if any.
Texture2DD3D9::~Texture2DD3D9() {
  DCHECK(d3d_texture_);
  d3d_texture_ = NULL;
  if (d3d_shadow_) {
    d3d_shadow_->Release();
    d3d_shadow_ = NULL;
  }
}

// Creates a 2D texture. For dynamic textures, create it in the default pool,
// and a shadow version in the system memory pool (that we can lock). For
// regular texture, simply create one in the managed pool.
Texture2DD3D9 *Texture2DD3D9::Create(GAPID3D9 *gapi,
                                     unsigned int width,
                                     unsigned int height,
                                     unsigned int levels,
                                     texture::Format format,
                                     unsigned int flags,
                                     bool enable_render_surfaces) {
  DCHECK_GT(width, 0U);
  DCHECK_GT(height, 0U);
  DCHECK_GT(levels, 0U);
  D3DFORMAT d3d_format = D3DFormat(format);
  IDirect3DDevice9 *device = gapi->d3d_device();
  if (enable_render_surfaces) {
    CComPtr<IDirect3DTexture9> d3d_texture = NULL;
    HRESULT result = device->CreateTexture(width, height, levels,
                                           D3DUSAGE_RENDERTARGET, d3d_format,
                                           D3DPOOL_DEFAULT, &d3d_texture,
                                           NULL);
    if (result != D3D_OK) {
      LOG(ERROR) << "DirectX error when calling CreateTexture: "
                 << DXGetErrorStringA(result);
      return NULL;
    }
    return new Texture2DD3D9(levels, format, flags, width, height, d3d_texture,
                             NULL, enable_render_surfaces);
  } else if (flags & texture::kDynamic) {
    CComPtr<IDirect3DTexture9> d3d_texture = NULL;
    HRESULT result = device->CreateTexture(width, height, levels,
                                           D3DUSAGE_DYNAMIC, d3d_format,
                                           D3DPOOL_DEFAULT, &d3d_texture,
                                           NULL);
    if (result != D3D_OK) {
      LOG(ERROR) << "DirectX error when calling CreateTexture: "
                 << DXGetErrorStringA(result);
      return NULL;
    }
    CComPtr<IDirect3DTexture9> d3d_shadow = NULL;
    result = device->CreateTexture(width, height, levels, D3DUSAGE_DYNAMIC,
                                   d3d_format, D3DPOOL_SYSTEMMEM, &d3d_shadow,
                                   NULL);
    if (result != D3D_OK) {
      LOG(ERROR) << "DirectX error when calling CreateTexture: "
                 << DXGetErrorStringA(result);
      return NULL;
    }
    return new Texture2DD3D9(levels, format, flags, width, height, d3d_texture,
                             d3d_shadow, enable_render_surfaces);
  } else {
    CComPtr<IDirect3DTexture9> d3d_texture = NULL;
    HRESULT result = device->CreateTexture(width, height, levels, 0, d3d_format,
                                           D3DPOOL_MANAGED, &d3d_texture, NULL);
    if (result != D3D_OK) {
      LOG(ERROR) << "DirectX error when calling CreateTexture: "
                 << DXGetErrorStringA(result);
      return NULL;
    }
    return new Texture2DD3D9(levels, format, flags, width, height, d3d_texture,
                             NULL, enable_render_surfaces);
  }
}

// Sets the data in the texture, using LockRect()/UnlockRect(). For dynamic
// textures, it copies the data to the shadow texture, then updates the actual
// texture. For regular texture, it directly modifies the actual texture.
bool Texture2DD3D9::SetData(GAPID3D9 *gapi,
                            const Volume &volume,
                            unsigned int level,
                            texture::Face face,
                            unsigned int row_pitch,
                            unsigned int slice_pitch,
                            unsigned int size,
                            const void *data) {
  DCHECK(d3d_texture_);
  IDirect3DTexture9 *lock_texture = d3d_shadow_ ? d3d_shadow_ : d3d_texture_;

  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format(), width_, height_, 1, level);
  TransferInfo src_transfer_info;
  MakeTransferInfo(&src_transfer_info, mip_info, volume, row_pitch,
                   slice_pitch);
  if (!CheckVolume(mip_info, volume) || level >= levels() ||
      size < src_transfer_info.total_size)
    return false;

  bool full_rect = IsFullVolume(mip_info, volume);
  D3DLOCKED_RECT locked_rect;
  RECT rect = {volume.x, volume.y, volume.x+volume.width,
               volume.y+volume.height};
  DWORD lock_flags =
      full_rect && (flags() & texture::kDynamic) ? D3DLOCK_DISCARD : 0;
  HR(lock_texture->LockRect(level, &locked_rect, full_rect ? NULL : &rect,
                            lock_flags));

  TransferInfo dst_transfer_info;
  MakeTransferInfo(&dst_transfer_info, mip_info, volume, locked_rect.Pitch,
                   slice_pitch);
  TransferVolume(volume, mip_info, dst_transfer_info, locked_rect.pBits,
                 src_transfer_info, data);

  HR(lock_texture->UnlockRect(level));
  if (d3d_shadow_) {
    HR(gapi->d3d_device()->UpdateTexture(d3d_shadow_, d3d_texture_));
  }
  return true;
}

// Gets the data from the texture, using LockRect()/UnlockRect(). For dynamic
// textures, it gets the data from the shadow texture, For regular texture, it
// gets it directly from the actual texture.
bool Texture2DD3D9::GetData(GAPID3D9 *gapi,
                            const Volume &volume,
                            unsigned int level,
                            texture::Face face,
                            unsigned int row_pitch,
                            unsigned int slice_pitch,
                            unsigned int size,
                            void *data) {
  DCHECK(d3d_texture_);
  IDirect3DTexture9 *lock_texture = d3d_shadow_ ? d3d_shadow_ : d3d_texture_;

  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format(), width_, height_, 1, level);
  TransferInfo dst_transfer_info;
  MakeTransferInfo(&dst_transfer_info, mip_info, volume, row_pitch,
                   slice_pitch);
  if (!CheckVolume(mip_info, volume) || level >= levels() ||
      size < dst_transfer_info.total_size)
    return false;

  bool full_rect = IsFullVolume(mip_info, volume);
  D3DLOCKED_RECT locked_rect;
  RECT rect = {volume.x, volume.y, volume.x+volume.width,
               volume.y+volume.height};
  DWORD lock_flags = D3DLOCK_READONLY;
  HR(lock_texture->LockRect(level, &locked_rect, full_rect ? NULL : &rect,
                            lock_flags));
  TransferInfo src_transfer_info;
  MakeTransferInfo(&src_transfer_info, mip_info, volume, locked_rect.Pitch,
                   slice_pitch);
  TransferVolume(volume, mip_info, dst_transfer_info, data,
                 src_transfer_info, locked_rect.pBits);
  HR(lock_texture->UnlockRect(level));
  return true;
}

bool Texture2DD3D9::CreateRenderSurface(int width,
                                        int height,
                                        int mip_level,
                                        int side,
                                        IDirect3DSurface9** direct3d_surface) {
  IDirect3DTexture9* d3d_texture =
      static_cast<IDirect3DTexture9*>(d3d_base_texture());
  D3DSURFACE_DESC surface_desc;
  d3d_texture->GetLevelDesc(mip_level, &surface_desc);
  if (width != surface_desc.Width || height != surface_desc.Height) {
    return false;
  }
  HR(d3d_texture->GetSurfaceLevel(mip_level, direct3d_surface));
  return true;
}

// Texture 3D functions

// Destroys the 3D texture.
Texture3DD3D9::~Texture3DD3D9() {
  DCHECK(d3d_texture_);
  d3d_texture_ = NULL;
  if (d3d_shadow_) {
    d3d_shadow_->Release();
    d3d_shadow_ = NULL;
  }
}

// Creates a 3D texture. For dynamic textures, create it in the default pool,
// and a shadow version in the system memory pool (that we can lock). For
// regular texture, simply create one in the managed pool.
Texture3DD3D9 *Texture3DD3D9::Create(GAPID3D9 *gapi,
                                     unsigned int width,
                                     unsigned int height,
                                     unsigned int depth,
                                     unsigned int levels,
                                     texture::Format format,
                                     unsigned int flags,
                                     bool enable_render_surfaces) {
  DCHECK_GT(width, 0U);
  DCHECK_GT(height, 0U);
  DCHECK_GT(depth, 0U);
  DCHECK_GT(levels, 0U);
  D3DFORMAT d3d_format = D3DFormat(format);
  IDirect3DDevice9 *device = gapi->d3d_device();
  if (enable_render_surfaces) {
    CComPtr<IDirect3DVolumeTexture9> d3d_texture = NULL;
    HRESULT result = device->CreateVolumeTexture(width, height, depth, levels,
                                                 D3DUSAGE_RENDERTARGET,
                                                 d3d_format, D3DPOOL_DEFAULT,
                                                 &d3d_texture, NULL);
    if (result != D3D_OK) {
      LOG(ERROR) << "DirectX error when calling CreateTexture: "
                 << DXGetErrorStringA(result);
      return NULL;
    }
    return new Texture3DD3D9(levels, format, flags, width, height, depth,
                             d3d_texture, NULL, enable_render_surfaces);
  } else if (flags & texture::kDynamic) {
    CComPtr<IDirect3DVolumeTexture9> d3d_texture = NULL;
    HRESULT result = device->CreateVolumeTexture(width, height, depth, levels,
                                                 D3DUSAGE_DYNAMIC, d3d_format,
                                                 D3DPOOL_DEFAULT, &d3d_texture,
                                                 NULL);
    if (result != D3D_OK) {
      LOG(ERROR) << "DirectX error when calling CreateTexture: "
                 << DXGetErrorStringA(result);
      return NULL;
    }
    CComPtr<IDirect3DVolumeTexture9> d3d_shadow = NULL;
    result = device->CreateVolumeTexture(width, height, depth, levels,
                                         D3DUSAGE_DYNAMIC, d3d_format,
                                         D3DPOOL_SYSTEMMEM, &d3d_shadow, NULL);
    if (result != D3D_OK) {
      LOG(ERROR) << "DirectX error when calling CreateTexture: "
                 << DXGetErrorStringA(result);
      return NULL;
    }
    return new Texture3DD3D9(levels, format, flags, width, height, depth,
                             d3d_texture, d3d_shadow, enable_render_surfaces);
  } else {
    CComPtr<IDirect3DVolumeTexture9> d3d_texture = NULL;
    HRESULT result = device->CreateVolumeTexture(width, height, depth, levels,
                                                 D3DUSAGE_DYNAMIC, d3d_format,
                                                 D3DPOOL_MANAGED, &d3d_texture,
                                                 NULL);
    if (result != D3D_OK) {
      LOG(ERROR) << "DirectX error when calling CreateTexture: "
                 << DXGetErrorStringA(result);
      return NULL;
    }
    return new Texture3DD3D9(levels, format, flags, width, height, depth,
                             d3d_texture, NULL, enable_render_surfaces);
  }
}

// Sets the data in the texture, using LockBox()/UnlockBox(). For dynamic
// textures, it copies the data to the shadow texture, then updates the actual
// texture. For regular texture, it directly modifies the actual texture.
bool Texture3DD3D9::SetData(GAPID3D9 *gapi,
                            const Volume &volume,
                            unsigned int level,
                            texture::Face face,
                            unsigned int row_pitch,
                            unsigned int slice_pitch,
                            unsigned int size,
                            const void *data) {
  DCHECK(d3d_texture_);
  IDirect3DVolumeTexture9 *lock_texture =
      d3d_shadow_ ? d3d_shadow_ : d3d_texture_;

  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format(), width_, height_, depth_, level);
  TransferInfo src_transfer_info;
  MakeTransferInfo(&src_transfer_info, mip_info, volume, row_pitch,
                   slice_pitch);
  if (!CheckVolume(mip_info, volume) || level >= levels() ||
      size < src_transfer_info.total_size)
    return false;

  bool full_box = IsFullVolume(mip_info, volume);
  D3DLOCKED_BOX locked_box;
  D3DBOX box = {volume.x, volume.y, volume.z, volume.x+volume.width,
                volume.y+volume.height, volume.z+volume.depth};
  DWORD lock_flags =
      full_box && (flags() & texture::kDynamic) ? D3DLOCK_DISCARD : 0;
  HR(lock_texture->LockBox(level, &locked_box, full_box ? NULL : &box,
                           lock_flags));

  TransferInfo dst_transfer_info;
  MakeTransferInfo(&dst_transfer_info, mip_info, volume, locked_box.RowPitch,
                   locked_box.SlicePitch);
  TransferVolume(volume, mip_info, dst_transfer_info, locked_box.pBits,
                 src_transfer_info, data);

  HR(lock_texture->UnlockBox(level));
  if (d3d_shadow_) {
    HR(gapi->d3d_device()->UpdateTexture(d3d_shadow_, d3d_texture_));
  }
  return true;
}

// Gets the data from the texture, using LockBox()/UnlockBox(). For dynamic
// textures, it gets the data from the shadow texture, For regular texture, it
// gets it directly from the actual texture.
bool Texture3DD3D9::GetData(GAPID3D9 *gapi,
                            const Volume &volume,
                            unsigned int level,
                            texture::Face face,
                            unsigned int row_pitch,
                            unsigned int slice_pitch,
                            unsigned int size,
                            void *data) {
  DCHECK(d3d_texture_);
  IDirect3DVolumeTexture9 *lock_texture =
      d3d_shadow_ ? d3d_shadow_ : d3d_texture_;

  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format(), width_, height_, depth_, level);
  TransferInfo dst_transfer_info;
  MakeTransferInfo(&dst_transfer_info, mip_info, volume, row_pitch,
                   slice_pitch);
  if (!CheckVolume(mip_info, volume) || level >= levels() ||
      size < dst_transfer_info.total_size)
    return false;

  bool full_box = IsFullVolume(mip_info, volume);
  D3DLOCKED_BOX locked_box;
  D3DBOX box = {volume.x, volume.y, volume.z, volume.x+volume.width,
                volume.y+volume.height, volume.z+volume.depth};
  DWORD lock_flags = D3DLOCK_READONLY;
  HR(lock_texture->LockBox(level, &locked_box, full_box ? NULL : &box,
                           lock_flags));
  TransferInfo src_transfer_info;
  MakeTransferInfo(&src_transfer_info, mip_info, volume, locked_box.RowPitch,
                   locked_box.SlicePitch);
  TransferVolume(volume, mip_info, dst_transfer_info, data,
                 src_transfer_info, locked_box.pBits);
  HR(lock_texture->UnlockBox(level));
  return true;
}

bool Texture3DD3D9::CreateRenderSurface(int width,
                                        int height,
                                        int mip_level,
                                        int side,
                                        IDirect3DSurface9** direct3d_surface) {
  // TODO(rlp): Currently unsupported.
  DCHECK(false);
  return false;
}

// Texture Cube functions.

// Destroys the cube map texture, releasing the D3D texture, and its shadow if
// any.
TextureCubeD3D9::~TextureCubeD3D9() {
  DCHECK(d3d_texture_);
  d3d_texture_ = NULL;
  if (d3d_shadow_) {
    d3d_shadow_->Release();
    d3d_shadow_ = NULL;
  }
}

// Creates a cube map texture. For dynamic textures, create it in the default
// pool, and a shadow version in the system memory pool (that we can lock). For
// regular texture, simply create one in the managed pool.
TextureCubeD3D9 *TextureCubeD3D9::Create(GAPID3D9 *gapi,
                                         unsigned int side,
                                         unsigned int levels,
                                         texture::Format format,
                                         unsigned int flags,
                                         bool enable_render_surfaces) {
  DCHECK_GT(side, 0U);
  DCHECK_GT(levels, 0U);
  D3DFORMAT d3d_format = D3DFormat(format);
  IDirect3DDevice9 *device = gapi->d3d_device();
  if (enable_render_surfaces) {
    CComPtr<IDirect3DCubeTexture9> d3d_texture = NULL;
    HRESULT result = device->CreateCubeTexture(side, levels,
                                               D3DUSAGE_RENDERTARGET,
                                               d3d_format, D3DPOOL_DEFAULT,
                                               &d3d_texture, NULL);
    if (result != D3D_OK) {
      LOG(ERROR) << "DirectX error when calling CreateTexture: "
                 << DXGetErrorStringA(result);
      return NULL;
    }
    return new TextureCubeD3D9(levels, format, flags, side, d3d_texture, NULL,
                               enable_render_surfaces);
  } else if (flags & texture::kDynamic) {
    CComPtr<IDirect3DCubeTexture9> d3d_texture = NULL;
    HRESULT result = device->CreateCubeTexture(side, levels, D3DUSAGE_DYNAMIC,
                                               d3d_format, D3DPOOL_DEFAULT,
                                               &d3d_texture, NULL);
    if (result != D3D_OK) {
      LOG(ERROR) << "DirectX error when calling CreateTexture: "
                 << DXGetErrorStringA(result);
      return NULL;
    }
    CComPtr<IDirect3DCubeTexture9> d3d_shadow = NULL;
    result = device->CreateCubeTexture(side, levels, D3DUSAGE_DYNAMIC,
                                       d3d_format, D3DPOOL_SYSTEMMEM,
                                       &d3d_shadow, NULL);
    if (result != D3D_OK) {
      LOG(ERROR) << "DirectX error when calling CreateTexture: "
                 << DXGetErrorStringA(result);
      return NULL;
    }
    return new TextureCubeD3D9(levels, format, flags, side, d3d_texture,
                               d3d_shadow, enable_render_surfaces);
  } else {
    CComPtr<IDirect3DCubeTexture9> d3d_texture = NULL;
    HRESULT result = device->CreateCubeTexture(side, levels, 0, d3d_format,
                                               D3DPOOL_MANAGED, &d3d_texture,
                                               NULL);
    if (result != D3D_OK) {
      LOG(ERROR) << "DirectX error when calling CreateTexture: "
                 << DXGetErrorStringA(result);
      return NULL;
    }
    return new TextureCubeD3D9(levels, format, flags, side, d3d_texture, NULL,
                               enable_render_surfaces);
  }
}

// Sets the data in the texture, using LockRect()/UnlockRect(). For dynamic
// textures, it copies the data to the shadow texture, then updates the actual
// texture. For regular texture, it directly modifies the actual texture.
bool TextureCubeD3D9::SetData(GAPID3D9 *gapi,
                              const Volume &volume,
                              unsigned int level,
                              texture::Face face,
                              unsigned int row_pitch,
                              unsigned int slice_pitch,
                              unsigned int size,
                              const void *data) {
  DCHECK(d3d_texture_);
  IDirect3DCubeTexture9 *lock_texture =
      d3d_shadow_ ? d3d_shadow_ : d3d_texture_;
  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format(), side_, side_, 1, level);
  TransferInfo src_transfer_info;
  MakeTransferInfo(&src_transfer_info, mip_info, volume, row_pitch,
                   slice_pitch);
  if (!CheckVolume(mip_info, volume) || level >= levels() ||
      size < src_transfer_info.total_size)
    return false;

  D3DCUBEMAP_FACES d3d_face = D3DFace(face);
  bool full_rect = IsFullVolume(mip_info, volume);
  D3DLOCKED_RECT locked_rect;
  RECT rect = {volume.x, volume.y, volume.x+volume.width,
               volume.y+volume.height};
  DWORD lock_flags =
      full_rect && (flags() & texture::kDynamic) ? D3DLOCK_DISCARD : 0;
  HR(lock_texture->LockRect(d3d_face, level, &locked_rect,
                            full_rect ? NULL : &rect, lock_flags));

  TransferInfo dst_transfer_info;
  MakeTransferInfo(&dst_transfer_info, mip_info, volume, locked_rect.Pitch,
                   slice_pitch);
  TransferVolume(volume, mip_info, dst_transfer_info, locked_rect.pBits,
                 src_transfer_info, data);

  HR(lock_texture->UnlockRect(d3d_face, level));
  if (d3d_shadow_) {
    HR(gapi->d3d_device()->UpdateTexture(d3d_shadow_, d3d_texture_));
  }
  return true;
}

// Gets the data from the texture, using LockRect()/UnlockRect(). For dynamic
// textures, it gets the data from the shadow texture, For regular texture, it
// gets it directly from the actual texture.
bool TextureCubeD3D9::GetData(GAPID3D9 *gapi,
                              const Volume &volume,
                              unsigned int level,
                              texture::Face face,
                              unsigned int row_pitch,
                              unsigned int slice_pitch,
                              unsigned int size,
                              void *data) {
  DCHECK(d3d_texture_);
  IDirect3DCubeTexture9 *lock_texture =
      d3d_shadow_ ? d3d_shadow_ : d3d_texture_;

  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format(), side_, side_, 1, level);
  TransferInfo dst_transfer_info;
  MakeTransferInfo(&dst_transfer_info, mip_info, volume, row_pitch,
                   slice_pitch);
  if (!CheckVolume(mip_info, volume) || level >= levels() ||
      size < dst_transfer_info.total_size)
    return false;

  D3DCUBEMAP_FACES d3d_face = D3DFace(face);
  bool full_rect = IsFullVolume(mip_info, volume);
  D3DLOCKED_RECT locked_rect;
  RECT rect = {volume.x, volume.y, volume.x+volume.width,
               volume.y+volume.height};
  DWORD lock_flags = D3DLOCK_READONLY;
  HR(lock_texture->LockRect(d3d_face, level, &locked_rect,
                            full_rect ? NULL : &rect, lock_flags));
  TransferInfo src_transfer_info;
  MakeTransferInfo(&src_transfer_info, mip_info, volume, locked_rect.Pitch,
                   slice_pitch);
  TransferVolume(volume, mip_info, dst_transfer_info, data,
                 src_transfer_info, locked_rect.pBits);
  HR(lock_texture->UnlockRect(d3d_face, level));
  return true;
}

bool TextureCubeD3D9::CreateRenderSurface(
    int width,
    int height,
    int mip_level,
    int side,
    IDirect3DSurface9** direct3d_surface) {
  IDirect3DCubeTexture9* d3d_cube_texture =
      static_cast<IDirect3DCubeTexture9*>(d3d_base_texture());
  D3DSURFACE_DESC surface_desc;
  d3d_cube_texture->GetLevelDesc(mip_level, &surface_desc);
  if (width != surface_desc.Width || height != surface_desc.Height ||
      side < 0 || side > 5) {
    return false;
  }
  HR(d3d_cube_texture->GetCubeMapSurface(
      D3DFace(static_cast<texture::Face>(side)),
      mip_level,
      direct3d_surface));
  return true;
}

// GAPID3D9 functions.

// Destroys a texture resource.
parse_error::ParseError GAPID3D9::DestroyTexture(ResourceId id) {
  // Dirty effect, because this texture id may be used
  DirtyEffect();
  return textures_.Destroy(id) ?
      parse_error::kParseNoError :
      parse_error::kParseInvalidArguments;
}

// Creates a 2D texture resource.
parse_error::ParseError GAPID3D9::CreateTexture2D(
    ResourceId id,
    unsigned int width,
    unsigned int height,
    unsigned int levels,
    texture::Format format,
    unsigned int flags,
    bool enable_render_surfaces) {
  Texture2DD3D9 *texture = Texture2DD3D9::Create(this, width, height, levels,
                                                 format, flags,
                                                 enable_render_surfaces);
  if (!texture) return parse_error::kParseInvalidArguments;
  // Dirty effect, because this texture id may be used
  DirtyEffect();
  textures_.Assign(id, texture);
  return parse_error::kParseNoError;
}

// Creates a 3D texture resource.
parse_error::ParseError GAPID3D9::CreateTexture3D(
    ResourceId id,
    unsigned int width,
    unsigned int height,
    unsigned int depth,
    unsigned int levels,
    texture::Format format,
    unsigned int flags,
    bool enable_render_surfaces) {
  Texture3DD3D9 *texture = Texture3DD3D9::Create(this, width, height, depth,
                                                 levels, format, flags,
                                                 enable_render_surfaces);
  if (!texture) return parse_error::kParseInvalidArguments;
  // Dirty effect, because this texture id may be used
  DirtyEffect();
  textures_.Assign(id, texture);
  return parse_error::kParseNoError;
}

// Creates a cube map texture resource.
parse_error::ParseError GAPID3D9::CreateTextureCube(
    ResourceId id,
    unsigned int side,
    unsigned int levels,
    texture::Format format,
    unsigned int flags,
    bool enable_render_surfaces) {
  TextureCubeD3D9 *texture = TextureCubeD3D9::Create(this, side, levels,
                                                     format, flags,
                                                     enable_render_surfaces);
  if (!texture) return parse_error::kParseInvalidArguments;
  // Dirty effect, because this texture id may be used
  DirtyEffect();
  textures_.Assign(id, texture);
  return parse_error::kParseNoError;
}

// Copies the data into a texture resource.
parse_error::ParseError GAPID3D9::SetTextureData(
    ResourceId id,
    unsigned int x,
    unsigned int y,
    unsigned int z,
    unsigned int width,
    unsigned int height,
    unsigned int depth,
    unsigned int level,
    texture::Face face,
    unsigned int row_pitch,
    unsigned int slice_pitch,
    unsigned int size,
    const void *data) {
  TextureD3D9 *texture = textures_.Get(id);
  if (!texture)
    return parse_error::kParseInvalidArguments;
  Volume volume = {x, y, z, width, height, depth};
  return texture->SetData(this, volume, level, face, row_pitch, slice_pitch,
                          size, data) ?
      parse_error::kParseNoError :
      parse_error::kParseInvalidArguments;
}

// Copies the data from a texture resource.
parse_error::ParseError GAPID3D9::GetTextureData(
    ResourceId id,
    unsigned int x,
    unsigned int y,
    unsigned int z,
    unsigned int width,
    unsigned int height,
    unsigned int depth,
    unsigned int level,
    texture::Face face,
    unsigned int row_pitch,
    unsigned int slice_pitch,
    unsigned int size,
    void *data) {
  TextureD3D9 *texture = textures_.Get(id);
  if (!texture)
    return parse_error::kParseInvalidArguments;
  Volume volume = {x, y, z, width, height, depth};
  return texture->GetData(this, volume, level, face, row_pitch, slice_pitch,
                          size, data) ?
      parse_error::kParseNoError :
      parse_error::kParseInvalidArguments;
}

}  // namespace o3d
}  // namespace command_buffer
