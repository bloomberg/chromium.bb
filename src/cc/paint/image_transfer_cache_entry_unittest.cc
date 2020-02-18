// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/create_gr_gl_interface.h"
#include "ui/gl/init/gl_factory.h"

namespace cc {
namespace {

constexpr SkYUVColorSpace kJpegYUVColorSpace =
    SkYUVColorSpace::kJPEG_SkYUVColorSpace;

void MarkTextureAsReleased(SkImage::ReleaseContext context) {
  auto* released = static_cast<bool*>(context);
  DCHECK(!*released);
  *released = true;
}

// Checks if all the pixels in the |subset| of |image| are |expected_color|.
bool CheckRectIsSolidColor(const sk_sp<SkImage>& image,
                           SkColor expected_color,
                           const SkIRect& subset) {
  DCHECK_GE(image->width(), 1);
  DCHECK_GE(image->height(), 1);
  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(image->width(), image->height()))
    return false;
  SkPixmap pixmap;
  if (!bitmap.peekPixels(&pixmap))
    return false;
  if (!image->readPixels(pixmap, 0 /* srcX */, 0 /* srcY */))
    return false;
  for (int y = subset.fTop; y < subset.fBottom; y++) {
    for (int x = subset.fLeft; x < subset.fRight; x++) {
      if (bitmap.getColor(x, y) != expected_color)
        return false;
    }
  }
  return true;
}

// Checks if all the pixels in |image| are |expected_color|.
bool CheckImageIsSolidColor(const sk_sp<SkImage>& image,
                            SkColor expected_color) {
  return CheckRectIsSolidColor(
      image, expected_color, SkIRect::MakeWH(image->width(), image->height()));
}

class ImageTransferCacheEntryTest
    : public testing::TestWithParam<YUVDecodeFormat> {
 public:
  void SetUp() override {
    // Initialize a GL GrContext for Skia.
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    ASSERT_TRUE(surface_);
    share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
    gl_context_ = base::MakeRefCounted<gl::GLContextEGL>(share_group_.get());
    ASSERT_TRUE(gl_context_);
    ASSERT_TRUE(
        gl_context_->Initialize(surface_.get(), gl::GLContextAttribs()));
    ASSERT_TRUE(gl_context_->MakeCurrent(surface_.get()));
    sk_sp<GrGLInterface> interface(gl::init::CreateGrGLInterface(
        *gl_context_->GetVersionInfo(), false /* use_version_es2 */));
    gr_context_ = GrContext::MakeGL(std::move(interface));
    ASSERT_TRUE(gr_context_);
  }

  // Creates the textures for a 64x64 YUV 4:2:0 image where all the samples in
  // all planes are 255u. This corresponds to an RGB color of (255, 121, 255)
  // assuming the JPEG YUV-to-RGB conversion formulas. Returns a list of
  // SkImages backed by the textures. Note that the number of textures depends
  // on the format (obtained using GetParam()). |release_flags| is set to a list
  // of boolean flags initialized to false. Each flag corresponds to a plane (in
  // the same order as the returned SkImages). When the texture for that plane
  // is released by Skia, that flag will be set to true. Returns an empty vector
  // on failure.
  std::vector<sk_sp<SkImage>> CreateTestYUVImage(
      std::unique_ptr<bool[]>* release_flags) {
    std::vector<sk_sp<SkImage>> plane_images;
    *release_flags = nullptr;
    if (GetParam() == YUVDecodeFormat::kYUV3 ||
        GetParam() == YUVDecodeFormat::kYVU3) {
      *release_flags =
          std::unique_ptr<bool[]>(new bool[3]{false, false, false});
      plane_images = {
          CreateSolidPlane(gr_context(), 64, 64, GL_R8_EXT, SkColors::kWhite,
                           release_flags->get()),
          CreateSolidPlane(gr_context(), 32, 32, GL_R8_EXT, SkColors::kWhite,
                           release_flags->get() + 1),
          CreateSolidPlane(gr_context(), 32, 32, GL_R8_EXT, SkColors::kWhite,
                           release_flags->get() + 2)};
    } else if (GetParam() == YUVDecodeFormat::kYUV2) {
      *release_flags = std::unique_ptr<bool[]>(new bool[2]{false, false});
      plane_images = {
          CreateSolidPlane(gr_context(), 64, 64, GL_R8_EXT, SkColors::kWhite,
                           release_flags->get()),
          CreateSolidPlane(gr_context(), 32, 32, GL_RG8_EXT, SkColors::kWhite,
                           release_flags->get() + 1)};
    } else {
      NOTREACHED();
      return {};
    }
    if (std::all_of(plane_images.cbegin(), plane_images.cend(),
                    [](sk_sp<SkImage> plane) { return !!plane; })) {
      return plane_images;
    }
    return {};
  }

  void DeletePendingTextures() {
    DCHECK(gr_context_);
    for (const auto& texture : textures_to_free_) {
      if (texture.isValid())
        gr_context_->deleteBackendTexture(texture);
    }
    gr_context_->flush();
    textures_to_free_.clear();
  }

  void TearDown() override {
    DeletePendingTextures();
    gr_context_.reset();
    surface_->PrepareToDestroy(gl_context_->IsCurrent(surface_.get()));
    surface_.reset();
    gl_context_.reset();
    share_group_.reset();
  }

  GrContext* gr_context() const { return gr_context_.get(); }

 private:
  // Uploads a texture corresponding to a single plane in a YUV image. All the
  // samples in the plane are set to |color|. The texture is not owned by Skia:
  // when Skia doesn't need it anymore, MarkTextureAsReleased() will be called.
  sk_sp<SkImage> CreateSolidPlane(GrContext* gr_context,
                                  int width,
                                  int height,
                                  GrGLenum texture_format,
                                  const SkColor4f& color,
                                  bool* released) {
    GrBackendTexture allocated_texture = gr_context->createBackendTexture(
        width, height, GrBackendFormat::MakeGL(texture_format, GL_TEXTURE_2D),
        color, GrMipMapped::kNo, GrRenderable::kNo);
    if (!allocated_texture.isValid())
      return nullptr;
    textures_to_free_.push_back(allocated_texture);
    GrGLTextureInfo allocated_texture_info;
    if (!allocated_texture.getGLTextureInfo(&allocated_texture_info))
      return nullptr;
    DCHECK_EQ(width, allocated_texture.width());
    DCHECK_EQ(height, allocated_texture.height());
    DCHECK(!allocated_texture.hasMipMaps());
    DCHECK(allocated_texture_info.fTarget == GL_TEXTURE_2D);
    if (texture_format == GL_RG8_EXT) {
      // TODO(crbug.com/985458): using GL_RGBA8_EXT is a workaround so that we
      // can make a SkImage out of a GL_RG8_EXT texture. Revisit this once Skia
      // supports a corresponding SkColorType.
      allocated_texture = GrBackendTexture(
          width, height, GrMipMapped::kNo,
          GrGLTextureInfo{GL_TEXTURE_2D, allocated_texture_info.fID,
                          GL_RGBA8_EXT});
    }
    *released = false;
    return SkImage::MakeFromTexture(
        gr_context, allocated_texture, kTopLeft_GrSurfaceOrigin,
        texture_format == GL_RG8_EXT ? kRGBA_8888_SkColorType
                                     : kAlpha_8_SkColorType,
        kOpaque_SkAlphaType, nullptr /* colorSpace */, MarkTextureAsReleased,
        released);
  }

  std::vector<GrBackendTexture> textures_to_free_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLContext> gl_context_;
  sk_sp<GrContext> gr_context_;
  gl::DisableNullDrawGLBindings enable_pixel_output_;
};

TEST_P(ImageTransferCacheEntryTest, Deserialize) {
#if defined(OS_ANDROID)
  // TODO(crbug.com/985458): this test is failing on Android for NV12 and we
  // don't understand why yet. Revisit this once Skia supports an RG8
  // SkColorType.
  if (GetParam() == YUVDecodeFormat::kYUV2)
    return;
#endif

  // Create a client-side entry from YUV planes. Use a different stride than the
  // width to test that alignment works correctly.
  const int image_width = 12;
  const int image_height = 10;
  const size_t y_stride = 16;
  const size_t uv_stride = 8;

  const size_t y_bytes = y_stride * image_height;
  const size_t uv_bytes = uv_stride * image_height / 2;
  const size_t planes_size = y_bytes + 2 * uv_bytes;
  std::unique_ptr<char[]> planes_data(new char[planes_size]);

  void* planes[3];
  planes[0] = reinterpret_cast<void*>(planes_data.get());
  planes[1] = ((char*)planes[0]) + y_bytes;
  planes[2] = ((char*)planes[1]) + uv_bytes;

  auto info = SkImageInfo::Make(image_width, image_height, kGray_8_SkColorType,
                                kUnknown_SkAlphaType);
  SkPixmap y_pixmap(info, planes[0], y_stride);
  SkPixmap u_pixmap(info.makeWH(image_width / 2, image_height / 2), planes[1],
                    uv_stride);
  SkPixmap v_pixmap(info.makeWH(image_width / 2, image_height / 2), planes[2],
                    uv_stride);

  // rgb (255, 121, 255) -> yuv (255, 255, 255)
  const SkIRect bottom_color_rect =
      SkIRect::MakeXYWH(0, image_height / 2, image_width, image_height / 2);
  ASSERT_TRUE(y_pixmap.erase(SkColors::kWhite));
  ASSERT_TRUE(u_pixmap.erase(SkColors::kWhite));
  ASSERT_TRUE(v_pixmap.erase(SkColors::kWhite));
  // rgb (178, 0, 225) -> yuv (0, 255, 255)
  const SkIRect top_color_rect = SkIRect::MakeWH(image_width, image_height / 2);
  ASSERT_TRUE(y_pixmap.erase(SkColors::kBlack, &top_color_rect));

  auto client_entry(std::make_unique<ClientImageTransferCacheEntry>(
      &y_pixmap, &u_pixmap, &v_pixmap, nullptr, kJpegYUVColorSpace,
      true /* needs_mips */));
  uint32_t size = client_entry->SerializedSize();
  std::vector<uint8_t> data(size);
  ASSERT_TRUE(client_entry->Serialize(
      base::make_span(static_cast<uint8_t*>(data.data()), size)));

  // Create service-side entry from the client-side serialize info
  auto entry(std::make_unique<ServiceImageTransferCacheEntry>());
  ASSERT_TRUE(entry->Deserialize(
      gr_context(), base::make_span(static_cast<uint8_t*>(data.data()), size)));
  ASSERT_TRUE(entry->is_yuv());

  // Check color of pixels
  ASSERT_TRUE(CheckRectIsSolidColor(entry->image(), SkColorSetRGB(178, 0, 225),
                                    top_color_rect));
  ASSERT_TRUE(CheckRectIsSolidColor(
      entry->image(), SkColorSetRGB(255, 121, 255), bottom_color_rect));

  client_entry.reset();
  entry.reset();
}

TEST_P(ImageTransferCacheEntryTest, HardwareDecodedNoMipsAtCreation) {
  std::unique_ptr<bool[]> release_flags;
  std::vector<sk_sp<SkImage>> plane_images = CreateTestYUVImage(&release_flags);
  ASSERT_EQ(NumberOfPlanesForYUVDecodeFormat(GetParam()), plane_images.size());

  // Create a service-side image cache entry backed by these planes and do not
  // request generating mipmap chains. The |buffer_byte_size| is only used for
  // accounting, so we just set it to 0u.
  auto entry(std::make_unique<ServiceImageTransferCacheEntry>());
  EXPECT_TRUE(entry->BuildFromHardwareDecodedImage(
      gr_context(), std::move(plane_images),
      GetParam() /* plane_images_format */, kJpegYUVColorSpace,
      0u /* buffer_byte_size */, false /* needs_mips */));

  // We didn't request generating mipmap chains, so the textures we created
  // above should stay alive until after the cache entry is deleted.
  EXPECT_TRUE(std::none_of(release_flags.get(),
                           release_flags.get() + plane_images.size(),
                           [](bool released) { return released; }));
  entry.reset();
  EXPECT_TRUE(std::all_of(release_flags.get(),
                          release_flags.get() + plane_images.size(),
                          [](bool released) { return released; }));
}

TEST_P(ImageTransferCacheEntryTest, HardwareDecodedMipsAtCreation) {
#if defined(OS_ANDROID)
  // TODO(crbug.com/985458): this test is failing on Android for NV12 and we
  // don't understand why yet. Revisit this once Skia supports an RG8
  // SkColorType.
  if (GetParam() == YUVDecodeFormat::kYUV2)
    return;
#endif
  std::unique_ptr<bool[]> release_flags;
  std::vector<sk_sp<SkImage>> plane_images = CreateTestYUVImage(&release_flags);
  ASSERT_EQ(NumberOfPlanesForYUVDecodeFormat(GetParam()), plane_images.size());

  // Create a service-side image cache entry backed by these planes and request
  // generating mipmap chains at creation time. The |buffer_byte_size| is only
  // used for accounting, so we just set it to 0u.
  auto entry(std::make_unique<ServiceImageTransferCacheEntry>());
  EXPECT_TRUE(entry->BuildFromHardwareDecodedImage(
      gr_context(), std::move(plane_images),
      GetParam() /* plane_images_format */, kJpegYUVColorSpace,
      0u /* buffer_byte_size */, true /* needs_mips */));

  // We requested generating mipmap chains at creation time, so the textures we
  // created above should be released by now.
  EXPECT_TRUE(std::all_of(release_flags.get(),
                          release_flags.get() + plane_images.size(),
                          [](bool released) { return released; }));
  DeletePendingTextures();

  // Make sure that when we read the pixels from the YUV image, we get the
  // correct RGB color corresponding to the planes created previously. This
  // basically checks that after deleting the original YUV textures, the new
  // YUV image is backed by the correct mipped planes.
  ASSERT_TRUE(entry->image());
  EXPECT_TRUE(
      CheckImageIsSolidColor(entry->image(), SkColorSetRGB(255, 121, 255)));
}

TEST_P(ImageTransferCacheEntryTest, HardwareDecodedMipsAfterCreation) {
#if defined(OS_ANDROID)
  // TODO(crbug.com/985458): this test is failing on Android for NV12 and we
  // don't understand why yet. Revisit this once Skia supports an RG8
  // SkColorType.
  if (GetParam() == YUVDecodeFormat::kYUV2)
    return;
#endif
  std::unique_ptr<bool[]> release_flags;
  std::vector<sk_sp<SkImage>> plane_images = CreateTestYUVImage(&release_flags);
  ASSERT_EQ(NumberOfPlanesForYUVDecodeFormat(GetParam()), plane_images.size());

  // Create a service-side image cache entry backed by these planes and do not
  // request generating mipmap chains at creation time. The |buffer_byte_size|
  // is only used for accounting, so we just set it to 0u.
  auto entry(std::make_unique<ServiceImageTransferCacheEntry>());
  EXPECT_TRUE(entry->BuildFromHardwareDecodedImage(
      gr_context(), std::move(plane_images),
      GetParam() /* plane_images_format */, kJpegYUVColorSpace,
      0u /* buffer_byte_size */, false /* needs_mips */));

  // We didn't request generating mip chains, so the textures we created above
  // should stay alive for now.
  EXPECT_TRUE(std::none_of(release_flags.get(),
                           release_flags.get() + plane_images.size(),
                           [](bool released) { return released; }));

  // Now request generating the mip chains.
  entry->EnsureMips();

  // Now the original textures should have been released.
  EXPECT_TRUE(std::all_of(release_flags.get(),
                          release_flags.get() + plane_images.size(),
                          [](bool released) { return released; }));
  DeletePendingTextures();

  // Make sure that when we read the pixels from the YUV image, we get the
  // correct RGB color corresponding to the planes created previously. This
  // basically checks that after deleting the original YUV textures, the new
  // YUV image is backed by the correct mipped planes.
  ASSERT_TRUE(entry->image());
  EXPECT_TRUE(
      CheckImageIsSolidColor(entry->image(), SkColorSetRGB(255, 121, 255)));
}

std::string TestParamToString(
    const testing::TestParamInfo<YUVDecodeFormat>& param_info) {
  switch (param_info.param) {
    case YUVDecodeFormat::kYUV3:
      return "YUV3";
    case YUVDecodeFormat::kYVU3:
      return "YVU3";
    case YUVDecodeFormat::kYUV2:
      return "YUV2";
    default:
      NOTREACHED();
      return "";
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ImageTransferCacheEntryTest,
                         ::testing::Values(YUVDecodeFormat::kYUV3,
                                           YUVDecodeFormat::kYVU3,
                                           YUVDecodeFormat::kYUV2),
                         TestParamToString);

TEST(ImageTransferCacheEntryTestNoYUV, CPUImageWithMips) {
  GrMockOptions options;
  auto gr_context = GrContext::MakeMock(&options);

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(gr_context->maxTextureSize() + 1, 10));
  ClientImageTransferCacheEntry client_entry(&bitmap.pixmap(), nullptr, true);
  std::vector<uint8_t> storage(client_entry.SerializedSize());
  client_entry.Serialize(base::make_span(storage.data(), storage.size()));

  ServiceImageTransferCacheEntry service_entry;
  service_entry.Deserialize(gr_context.get(),
                            base::make_span(storage.data(), storage.size()));
  ASSERT_TRUE(service_entry.image());
  auto pre_mip_image = service_entry.image();
  EXPECT_FALSE(pre_mip_image->isTextureBacked());
  EXPECT_TRUE(service_entry.has_mips());

  service_entry.EnsureMips();
  ASSERT_TRUE(service_entry.image());
  EXPECT_FALSE(service_entry.image()->isTextureBacked());
  EXPECT_TRUE(service_entry.has_mips());
  EXPECT_EQ(pre_mip_image, service_entry.image());
}

TEST(ImageTransferCacheEntryTestNoYUV, CPUImageAddMipsLater) {
  GrMockOptions options;
  auto gr_context = GrContext::MakeMock(&options);

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(gr_context->maxTextureSize() + 1, 10));
  ClientImageTransferCacheEntry client_entry(&bitmap.pixmap(), nullptr, false);
  std::vector<uint8_t> storage(client_entry.SerializedSize());
  client_entry.Serialize(base::make_span(storage.data(), storage.size()));

  ServiceImageTransferCacheEntry service_entry;
  service_entry.Deserialize(gr_context.get(),
                            base::make_span(storage.data(), storage.size()));
  ASSERT_TRUE(service_entry.image());
  auto pre_mip_image = service_entry.image();
  EXPECT_FALSE(pre_mip_image->isTextureBacked());
  EXPECT_TRUE(service_entry.has_mips());

  service_entry.EnsureMips();
  ASSERT_TRUE(service_entry.image());
  EXPECT_FALSE(service_entry.image()->isTextureBacked());
  EXPECT_TRUE(service_entry.has_mips());
  EXPECT_EQ(pre_mip_image, service_entry.image());
}

}  // namespace
}  // namespace cc
