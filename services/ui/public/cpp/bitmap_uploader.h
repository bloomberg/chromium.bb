// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_BITMAP_UPLOADER_H_
#define SERVICES_UI_PUBLIC_CPP_BITMAP_UPLOADER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "gpu/GLES2/gl2chromium.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "services/ui/public/cpp/window_surface.h"
#include "services/ui/public/cpp/window_surface_client.h"
#include "services/ui/public/interfaces/surface.mojom.h"

namespace ui {
class GLES2Context;

extern const char kBitmapUploaderForAcceleratedWidget[];

// BitmapUploader is useful if you want to draw a bitmap or color in a
// Window.
class BitmapUploader : public WindowSurfaceClient {
 public:
  explicit BitmapUploader(Window* window);
  ~BitmapUploader() override;

  void Init();

  // Sets the color which is RGBA.
  void SetColor(uint32_t color);

  enum Format {
    RGBA,  // Pixel layout on Android.
    BGRA,  // Pixel layout everywhere else.
  };

  // Sets a bitmap.
  void SetBitmap(int width,
                 int height,
                 std::unique_ptr<std::vector<unsigned char>> data,
                 Format format);

 private:
  void Upload();

  uint32_t BindTextureForSize(const gfx::Size& size);

  uint32_t TextureFormat() const {
    return format_ == BGRA ? GL_BGRA_EXT : GL_RGBA;
  }

  void SetIdNamespace(uint32_t id_namespace);

  // WindowSurfaceClient implementation.
  void OnResourcesReturned(
      WindowSurface* surface,
      mojo::Array<cc::ReturnedResource> resources) override;

  ui::Window* window_;
  std::unique_ptr<ui::WindowSurface> surface_;
  // This may be null if there is an error contacting mus/initializing. We
  // assume we'll be shutting down soon and do nothing in this case.
  std::unique_ptr<ui::GLES2Context> gles2_context_;

  uint32_t color_;
  int width_;
  int height_;
  Format format_;
  std::unique_ptr<std::vector<unsigned char>> bitmap_;
  uint32_t next_resource_id_;
  base::hash_map<uint32_t, uint32_t> resource_to_texture_id_map_;

  DISALLOW_COPY_AND_ASSIGN(BitmapUploader);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_BITMAP_UPLOADER_H_
