// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/completion_event.h"
#include "cc/base/region.h"
#include "cc/layers/recording_source.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/raster/playback_image_provider.h"
#include "cc/raster/raster_source.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/tiles/gpu_image_decode_cache.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_implementation.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gl/gl_implementation.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace cc {
namespace {
scoped_refptr<DisplayItemList> MakeNoopDisplayItemList() {
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<SaveOp>();
  display_item_list->push<RestoreOp>();
  display_item_list->EndPaintOfUnpaired(gfx::Rect(10000, 10000));
  display_item_list->Finalize();
  return display_item_list;
}

constexpr size_t kCacheLimitBytes = 1024 * 1024;

class OopPixelTest : public testing::Test,
                     public gpu::raster::GrShaderCache::Client {
 public:
  OopPixelTest() : gr_shader_cache_(kCacheLimitBytes, this) {}

  void SetUp() override {
    InitializeOOPContext();
    // Needs RasterInterface for ScopedRasterContextLock
    gles2_context_provider_ =
        base::MakeRefCounted<viz::TestInProcessContextProvider>(
            /*enable_gles2_interface=*/true, /*support_locking=*/true,
            viz::RasterInterfaceType::GPU);
    gpu::ContextResult result = gles2_context_provider_->BindToCurrentThread();
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);
    const int gles2_max_texture_size =
        gles2_context_provider_->ContextCapabilities().max_texture_size;
    gpu_image_cache_ = std::make_unique<GpuImageDecodeCache>(
        gles2_context_provider_.get(), false, kRGBA_8888_SkColorType,
        kWorkingSetSize, gles2_max_texture_size,
        PaintImage::kDefaultGeneratorClientId, nullptr);

    const int raster_max_texture_size =
        raster_context_provider_->ContextCapabilities().max_texture_size;
    ASSERT_EQ(raster_max_texture_size, gles2_max_texture_size);
  }

  // gpu::raster::GrShaderCache::Client implementation.
  void StoreShader(const std::string& key, const std::string& shader) override {
  }

  void InitializeOOPContext() {
    if (oop_image_cache_)
      oop_image_cache_.reset();

    raster_context_provider_ =
        base::MakeRefCounted<viz::TestInProcessContextProvider>(
            /*enable_gles2_interface=*/false, /*support_locking=*/true,
            viz::RasterInterfaceType::OOPR, &gr_shader_cache_,
            &activity_flags_);
    gpu::ContextResult result = raster_context_provider_->BindToCurrentThread();
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);
    const int raster_max_texture_size =
        raster_context_provider_->ContextCapabilities().max_texture_size;
    oop_image_cache_ = std::make_unique<GpuImageDecodeCache>(
        raster_context_provider_.get(), true, kRGBA_8888_SkColorType,
        kWorkingSetSize, raster_max_texture_size,
        PaintImage::kDefaultGeneratorClientId, nullptr);
  }

  class RasterOptions {
   public:
    RasterOptions() = default;
    explicit RasterOptions(const gfx::Size& playback_size) {
      resource_size = playback_size;
      content_size = resource_size;
      full_raster_rect = gfx::Rect(playback_size);
      playback_rect = gfx::Rect(playback_size);
    }

    SkColor background_color = SK_ColorBLACK;
    int msaa_sample_count = 0;
    bool use_lcd_text = false;
    PlaybackImageProvider::RasterMode image_provider_raster_mode =
        PlaybackImageProvider::RasterMode::kSoftware;
    gfx::Size resource_size;
    gfx::Size content_size;
    gfx::Rect full_raster_rect;
    gfx::Rect playback_rect;
    gfx::Vector2dF post_translate = {0.f, 0.f};
    float post_scale = 1.f;
    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
    bool requires_clear = false;
    bool preclear = false;
    SkColor preclear_color;
    raw_ptr<ImageDecodeCache> image_cache = nullptr;
    std::vector<scoped_refptr<DisplayItemList>> additional_lists;
    raw_ptr<PaintShader> shader_with_animated_images = nullptr;
  };

  SkBitmap Raster(scoped_refptr<DisplayItemList> display_item_list,
                  const gfx::Size& playback_size) {
    RasterOptions options(playback_size);
    return Raster(display_item_list, options);
  }

  SkBitmap Raster(scoped_refptr<DisplayItemList> display_item_list,
                  const RasterOptions& options) {
    GURL url("https://example.com/foo");
    viz::TestInProcessContextProvider::ScopedRasterContextLock lock(
        raster_context_provider_.get(), url.possibly_invalid_spec().c_str());

    absl::optional<PlaybackImageProvider::Settings> settings;
    settings.emplace(PlaybackImageProvider::Settings());
    settings->raster_mode = options.image_provider_raster_mode;
    PlaybackImageProvider image_provider(
        oop_image_cache_.get(), options.color_space, std::move(settings));

    int width = options.resource_size.width();
    int height = options.resource_size.height();

    // Create and allocate a shared image on the raster interface.
    auto* raster_implementation = raster_context_provider_->RasterInterface();
    auto* sii = raster_context_provider_->SharedImageInterface();
    uint32_t flags = gpu::SHARED_IMAGE_USAGE_RASTER |
                     gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
    gpu::Mailbox mailbox = sii->CreateSharedImage(
        viz::ResourceFormat::RGBA_8888, gfx::Size(width, height),
        options.color_space, kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
        flags, gpu::kNullSurfaceHandle);
    EXPECT_TRUE(mailbox.Verify());
    raster_implementation->WaitSyncTokenCHROMIUM(
        sii->GenUnverifiedSyncToken().GetConstData());

    // Assume legacy MSAA if sample count is positive.
    gpu::raster::MsaaMode msaa_mode = options.msaa_sample_count > 0
                                          ? gpu::raster::kMSAA
                                          : gpu::raster::kNoMSAA;

    if (options.preclear) {
      raster_implementation->BeginRasterCHROMIUM(
          options.preclear_color, /*needs_clear=*/options.preclear,
          options.msaa_sample_count, msaa_mode, options.use_lcd_text,
          options.color_space, mailbox.name);
      raster_implementation->EndRasterCHROMIUM();
    }

    // "Out of process" raster! \o/
    // If |options.preclear| is true, the mailbox has already been cleared by
    // the BeginRasterCHROMIUM call above, and we want to test that it is indeed
    // cleared, so set |needs_clear| to false here.
    raster_implementation->BeginRasterCHROMIUM(
        options.background_color, /*needs_clear=*/!options.preclear,
        options.msaa_sample_count, msaa_mode, options.use_lcd_text,
        options.color_space, mailbox.name);
    size_t max_op_size_limit =
        gpu::raster::RasterInterface::kDefaultMaxOpSizeHint;
    raster_implementation->RasterCHROMIUM(
        display_item_list.get(), &image_provider, options.content_size,
        options.full_raster_rect, options.playback_rect, options.post_translate,
        gfx::Vector2dF(options.post_scale, options.post_scale),
        options.requires_clear, &max_op_size_limit);
    for (const auto& list : options.additional_lists) {
      raster_implementation->RasterCHROMIUM(
          list.get(), &image_provider, options.content_size,
          options.full_raster_rect, options.playback_rect,
          options.post_translate,
          gfx::Vector2dF(options.post_scale, options.post_scale),
          options.requires_clear, &max_op_size_limit);
    }
    raster_implementation->EndRasterCHROMIUM();
    raster_implementation->OrderingBarrierCHROMIUM();

    EXPECT_EQ(raster_implementation->GetError(),
              static_cast<unsigned>(GL_NO_ERROR));

    gpu::gles2::GLES2Interface* gl = gles2_context_provider_->ContextGL();
    SkBitmap result = ReadbackMailbox(gl, mailbox, options);
    gpu::SyncToken sync_token;
    gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
    sii->DestroySharedImage(sync_token, mailbox);
    return result;
  }

  SkBitmap ReadbackMailbox(gpu::gles2::GLES2Interface* gl,
                           const gpu::Mailbox& mailbox,
                           const RasterOptions& options) {
    // Import the texture in gl, create an fbo and bind the texture to it.
    GLuint gl_texture_id =
        gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name);
    gl->BeginSharedImageAccessDirectCHROMIUM(
        gl_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    GLuint fbo_id;
    gl->GenFramebuffers(1, &fbo_id);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_id);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, gl_texture_id, 0);

    // Read the data back.
    int width = options.resource_size.width();
    int height = options.resource_size.height();
    std::unique_ptr<unsigned char[]> data(
        new unsigned char[width * height * 4]);
    gl->ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.get());

    gl->DeleteFramebuffers(1, &fbo_id);

    gl->EndSharedImageAccessDirectCHROMIUM(gl_texture_id);
    gl->DeleteTextures(1, &gl_texture_id);

    // Swizzle rgba->bgra
    std::vector<SkPMColor> colors;
    colors.reserve(width * height);
    for (int h = 0; h < height; ++h) {
      for (int w = 0; w < width; ++w) {
        int i = (h * width + w) * 4;
        colors.push_back(SkPreMultiplyARGB(data[i + 3], data[i + 0],
                                           data[i + 1], data[i + 2]));
      }
    }

    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    SkPixmap pixmap(SkImageInfo::MakeN32Premul(width, height), colors.data(),
                    width * sizeof(SkColor));
    bitmap.writePixels(pixmap);
    return bitmap;
  }

  gpu::Mailbox CreateMailboxSharedImage(gpu::raster::RasterInterface* ri,
                                        gpu::SharedImageInterface* sii,
                                        const RasterOptions& options,
                                        viz::ResourceFormat image_format) {
    uint32_t flags = gpu::SHARED_IMAGE_USAGE_RASTER |
                     gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
    gpu::Mailbox mailbox = sii->CreateSharedImage(
        image_format, options.resource_size, options.color_space,
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, flags,
        gpu::kNullSurfaceHandle);
    EXPECT_TRUE(mailbox.Verify());
    ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

    return mailbox;
  }

  void UploadPixels(gpu::gles2::GLES2Interface* gl,
                    const gpu::Mailbox& mailbox,
                    const gfx::Size& size,
                    GLenum format,
                    GLenum type,
                    const void* data) {
    GLuint texture = gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name);
    gl->BindTexture(GL_TEXTURE_2D, texture);
    gl->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.width(), size.height(),
                      format, type, data);
    gl->BindTexture(GL_TEXTURE_2D, 0);
    gl->DeleteTextures(1, &texture);
  }

  SkBitmap RasterExpectedBitmap(
      scoped_refptr<DisplayItemList> display_item_list,
      const gfx::Size& playback_size) {
    RasterOptions options(playback_size);
    return RasterExpectedBitmap(display_item_list, options);
  }

  SkBitmap RasterExpectedBitmap(
      scoped_refptr<DisplayItemList> display_item_list,
      const RasterOptions& options) {
    viz::TestInProcessContextProvider::ScopedRasterContextLock lock(
        gles2_context_provider_.get());
    gles2_context_provider_->GrContext()->resetContext();

    // Generate bitmap via the "in process" raster path.  This verifies
    // that the preamble setup in RasterSource::PlaybackToCanvas matches
    // the same setup done in GLES2Implementation::RasterCHROMIUM.
    RecordingSource recording;
    recording.UpdateDisplayItemList(display_item_list, 1.f);
    recording.SetBackgroundColor(options.background_color);
    Region fake_invalidation;
    gfx::Rect layer_rect(gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom()));
    recording.UpdateAndExpandInvalidation(&fake_invalidation, layer_rect.size(),
                                          layer_rect);
    recording.SetRequiresClear(options.requires_clear);

    if (options.shader_with_animated_images)
      options.shader_with_animated_images->set_has_animated_images(true);

    PlaybackImageProvider image_provider(gpu_image_cache_.get(),
                                         options.color_space,
                                         PlaybackImageProvider::Settings());

    auto raster_source = recording.CreateRasterSource();
    RasterSource::PlaybackSettings settings;
    settings.use_lcd_text = options.use_lcd_text;
    settings.image_provider = &image_provider;

    uint32_t flags = 0;
    SkSurfaceProps surface_props(flags, kUnknown_SkPixelGeometry);
    if (options.use_lcd_text) {
      surface_props = SkSurfaceProps(flags, kRGB_H_SkPixelGeometry);
    }
    SkImageInfo image_info = SkImageInfo::MakeN32Premul(
        options.resource_size.width(), options.resource_size.height(),
        options.color_space.ToSkColorSpace());
    auto surface = SkSurface::MakeRenderTarget(
        gles2_context_provider_->GrContext(), SkBudgeted::kYes, image_info, 0,
        &surface_props);
    SkCanvas* canvas = surface->getCanvas();
    if (options.preclear)
      canvas->drawColor(options.preclear_color);
    else
      canvas->drawColor(options.background_color);

    gfx::AxisTransform2d raster_transform(options.post_scale,
                                          options.post_translate);
    raster_source->PlaybackToCanvas(
        canvas, options.content_size, options.full_raster_rect,
        options.playback_rect, raster_transform, settings);
    surface->flushAndSubmit();
    EXPECT_EQ(gles2_context_provider_->ContextGL()->GetError(),
              static_cast<unsigned>(GL_NO_ERROR));

    SkBitmap bitmap;
    SkImageInfo info = SkImageInfo::Make(
        options.resource_size.width(), options.resource_size.height(),
        SkColorType::kBGRA_8888_SkColorType, SkAlphaType::kPremul_SkAlphaType);
    bitmap.allocPixels(info, options.resource_size.width() * 4);
    bool success = surface->readPixels(bitmap, 0, 0);
    CHECK(success);
    EXPECT_EQ(gles2_context_provider_->ContextGL()->GetError(),
              static_cast<unsigned>(GL_NO_ERROR));
    return bitmap;
  }

  void ExpectEquals(SkBitmap actual,
                    SkBitmap expected,
                    const char* label = nullptr) {
    ExactPixelComparator exact(/* discard_alpha */ false);
    ExpectEquals(actual, expected, exact, label);
  }

  void ExpectEquals(SkBitmap actual,
                    SkBitmap expected,
                    const PixelComparator& comparator,
                    const char* label = nullptr) {
    EXPECT_EQ(actual.dimensions(), expected.dimensions());

    // We don't just use MatchesBitmap so that we can control logging output.
    if (comparator.Compare(actual, expected))
      return;

    auto expected_url = GetPNGDataUrl(expected);
    auto actual_url = GetPNGDataUrl(actual);
    if (label) {
      ADD_FAILURE() << "\nCase: " << label << "\nExpected: " << expected_url
                    << "\nActual:   " << actual_url;
    } else {
      ADD_FAILURE() << "\nExpected: " << expected_url
                    << "\nActual:   " << actual_url;
    }
  }

 protected:
  enum { kWorkingSetSize = 64 * 1024 * 1024 };
  scoped_refptr<viz::TestInProcessContextProvider> raster_context_provider_;
  scoped_refptr<viz::TestInProcessContextProvider> gles2_context_provider_;
  std::unique_ptr<GpuImageDecodeCache> gpu_image_cache_;
  std::unique_ptr<GpuImageDecodeCache> oop_image_cache_;
  gl::DisableNullDrawGLBindings enable_pixel_output_;
  std::unique_ptr<ImageProvider> image_provider_;
  int color_space_id_ = 0;
  gpu::raster::GrShaderCache gr_shader_cache_;
  gpu::GpuProcessActivityFlags activity_flags_;
};

class OopImagePixelTest : public OopPixelTest,
                          public ::testing::WithParamInterface<bool> {
 public:
  bool UseTooLargeImage() { return GetParam(); }
  PaintFlags::FilterQuality FilterQuality() {
    return PaintFlags::FilterQuality::kNone;
  }

  gfx::Size GetImageSize() {
    const int kMaxSize = 20000;
    DCHECK_GT(kMaxSize, gles2_context_provider_->GrContext()->maxTextureSize());
    return UseTooLargeImage() ? gfx::Size(10, kMaxSize) : gfx::Size(10, 10);
  }
};

class OopClearPixelTest : public OopPixelTest,
                          public ::testing::WithParamInterface<bool> {
 public:
  bool IsPartialRaster() const { return GetParam(); }
};

TEST_F(OopPixelTest, DrawColor) {
  gfx::Rect rect(10, 10);
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawColorOp>(SK_ColorBLUE, SkBlendMode::kSrc);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  std::vector<SkPMColor> expected_pixels(rect.width() * rect.height(),
                                         SkPreMultiplyARGB(255, 0, 0, 255));
  SkBitmap expected;
  expected.installPixels(
      SkImageInfo::MakeN32Premul(rect.width(), rect.height()),
      expected_pixels.data(), rect.width() * sizeof(SkColor));

  auto actual_oop = Raster(display_item_list, rect.size());
  ExpectEquals(actual_oop, expected, "oop");

  auto actual_gpu = RasterExpectedBitmap(display_item_list, rect.size());
  ExpectEquals(actual_gpu, expected, "gpu");
}

TEST_F(OopPixelTest, DrawColorWithTargetColorSpace) {
  gfx::Rect rect(10, 10);
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawColorOp>(SK_ColorBLUE, SkBlendMode::kSrc);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  gfx::ColorSpace target_color_space = gfx::ColorSpace::CreateXYZD50();

  RasterOptions options(rect.size());
  options.color_space = target_color_space;

  auto actual = Raster(display_item_list, options);
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);

  // Verify conversion.
  EXPECT_EQ(SkColorSetARGB(255, 38, 15, 221), expected.getColor(0, 0));
}

TEST_F(OopPixelTest, DrawRect) {
  gfx::Rect rect(10, 10);
  auto color_paint = [](int r, int g, int b) {
    PaintFlags flags;
    flags.setColor(SkColorSetARGB(255, r, g, b));
    return flags;
  };
  std::vector<std::pair<SkRect, PaintFlags>> input = {
      {SkRect::MakeXYWH(0, 0, 5, 5), color_paint(0, 0, 255)},
      {SkRect::MakeXYWH(5, 0, 5, 5), color_paint(0, 255, 0)},
      {SkRect::MakeXYWH(0, 5, 5, 5), color_paint(0, 255, 255)},
      {SkRect::MakeXYWH(5, 5, 5, 5), color_paint(255, 0, 0)}};

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  for (auto& op : input) {
    display_item_list->StartPaint();
    display_item_list->push<DrawRectOp>(op.first, op.second);
    display_item_list->EndPaintOfUnpaired(
        gfx::ToEnclosingRect(gfx::SkRectToRectF(op.first)));
  }
  display_item_list->Finalize();

  auto actual = Raster(std::move(display_item_list), rect.size());

  // Expected colors are 5x5 rects of
  //  BLUE GREEN
  //  CYAN  RED
  std::vector<SkPMColor> expected_pixels(rect.width() * rect.height());
  for (int h = 0; h < rect.height(); ++h) {
    auto start = expected_pixels.begin() + h * rect.width();
    SkPMColor left_color = SkPreMultiplyColor(
        h < 5 ? input[0].second.getColor() : input[2].second.getColor());
    SkPMColor right_color = SkPreMultiplyColor(
        h < 5 ? input[1].second.getColor() : input[3].second.getColor());

    std::fill(start, start + 5, left_color);
    std::fill(start + 5, start + 10, right_color);
  }
  SkBitmap expected;
  expected.installPixels(
      SkImageInfo::MakeN32Premul(rect.width(), rect.height()),
      expected_pixels.data(), rect.width() * sizeof(SkPMColor));
  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, DrawRecordPaintFilterTranslatedBounds) {
  gfx::Size output_size(10, 10);

  // The paint record filter's ops would fill the right half of the image with
  // green, but its record bounds are configured to clip it to the bottom right
  // quarter of the output.
  PaintFlags internal_flags;
  internal_flags.setColor(SK_ColorGREEN);
  sk_sp<PaintOpBuffer> filter_buffer(new PaintOpBuffer);
  filter_buffer->push<DrawRectOp>(
      SkRect::MakeLTRB(output_size.width() / 2.f, 0.f, output_size.width(),
                       output_size.height()),
      internal_flags);
  sk_sp<RecordPaintFilter> record_filter = sk_make_sp<RecordPaintFilter>(
      filter_buffer,
      SkRect::MakeLTRB(output_size.width() / 2.f, output_size.height() / 2.f,
                       output_size.width(), output_size.height()));

  PaintFlags record_flags;
  record_flags.setImageFilter(record_filter);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawColorOp>(SK_ColorWHITE, SkBlendMode::kSrc);
  display_item_list->push<SaveLayerOp>(nullptr, &record_flags);
  display_item_list->push<RestoreOp>();
  display_item_list->EndPaintOfUnpaired(gfx::Rect(output_size));
  display_item_list->Finalize();

  SkImageInfo ii =
      SkImageInfo::MakeN32Premul(output_size.width(), output_size.height());
  SkBitmap expected;
  expected.allocPixels(ii, ii.minRowBytes());
  expected.eraseColor(SK_ColorWHITE);
  expected.erase(
      SK_ColorGREEN,
      SkIRect::MakeLTRB(output_size.width() / 2, output_size.height() / 2,
                        output_size.width(), output_size.height()));

  auto actual_oop = Raster(display_item_list, output_size);
  auto actual_gpu = RasterExpectedBitmap(display_item_list, output_size);
  ExpectEquals(actual_oop, expected);
  ExpectEquals(actual_gpu, expected);
}

TEST_P(OopImagePixelTest, DrawImage) {
  SCOPED_TRACE(base::StringPrintf("UseTooLargeImage: %d, FilterQuality: %d\n",
                                  UseTooLargeImage(), FilterQuality()));

  gfx::Rect rect(10, 10);
  gfx::Size image_size = GetImageSize();

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(image_size.width(), image_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorMAGENTA);
  SkPaint green;
  green.setColor(SK_ColorGREEN);
  canvas.drawRect(SkRect::MakeXYWH(1, 2, 3, 4), green);

  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
  const PaintImage::Id kSomeId = 32;
  auto builder =
      PaintImageBuilder::WithDefault().set_image(image, 0).set_id(kSomeId);
  auto paint_image = builder.TakePaintImage();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(FilterQuality()));

  display_item_list->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling,
                                       nullptr);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, rect.size());
  auto expected = RasterExpectedBitmap(display_item_list, rect.size());
  ExpectEquals(actual, expected);

  EXPECT_EQ(actual.getColor(0, 0), SK_ColorMAGENTA);
}

TEST_P(OopImagePixelTest, DrawImageScaled) {
  SCOPED_TRACE(base::StringPrintf("UseTooLargeImage: %d, FilterQuality: %d\n",
                                  UseTooLargeImage(), FilterQuality()));

  gfx::Rect rect(10, 10);
  gfx::Size image_size = GetImageSize();

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(image_size.width(), image_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorMAGENTA);
  SkPaint green;
  green.setColor(SK_ColorGREEN);
  canvas.drawRect(SkRect::MakeXYWH(1, 2, 3, 4), green);

  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
  auto builder = PaintImageBuilder::WithDefault().set_image(image, 0).set_id(
      PaintImage::GetNextId());
  auto paint_image = builder.TakePaintImage();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<ScaleOp>(0.5f, 0.5f);
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(FilterQuality()));
  display_item_list->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling,
                                       nullptr);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, rect.size());
  auto expected = RasterExpectedBitmap(display_item_list, rect.size());
  ExpectEquals(actual, expected);
}

TEST_P(OopImagePixelTest, DrawImageShaderScaled) {
  SCOPED_TRACE(base::StringPrintf("UseTooLargeImage: %d, FilterQuality: %d\n",
                                  UseTooLargeImage(), FilterQuality()));

  gfx::Rect rect(10, 10);
  gfx::Size image_size = GetImageSize();

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(image_size.width(), image_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorMAGENTA);
  SkPaint green;
  green.setColor(SK_ColorGREEN);
  canvas.drawRect(SkRect::MakeXYWH(1, 2, 3, 4), green);

  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
  auto builder = PaintImageBuilder::WithDefault().set_image(image, 0).set_id(
      PaintImage::GetNextId());
  auto paint_image = builder.TakePaintImage();
  auto paint_image_shader = PaintShader::MakeImage(
      paint_image, SkTileMode::kRepeat, SkTileMode::kRepeat, nullptr);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<ScaleOp>(0.5f, 0.5f);
  PaintFlags flags;
  flags.setShader(paint_image_shader);
  flags.setFilterQuality(FilterQuality());
  display_item_list->push<DrawRectOp>(gfx::RectToSkRect(rect), flags);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, rect.size());
  auto expected = RasterExpectedBitmap(display_item_list, rect.size());
  ExpectEquals(actual, expected);
}

TEST_P(OopImagePixelTest, DrawRecordShaderWithImageScaled) {
  SCOPED_TRACE(base::StringPrintf("UseTooLargeImage: %d, FilterQuality: %d\n",
                                  UseTooLargeImage(), FilterQuality()));

  gfx::Rect rect(10, 10);
  gfx::Size image_size = GetImageSize();

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(image_size.width(), image_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorMAGENTA);
  SkPaint green;
  green.setColor(SK_ColorGREEN);
  canvas.drawRect(SkRect::MakeXYWH(1, 2, 3, 4), green);

  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
  auto builder = PaintImageBuilder::WithDefault().set_image(image, 0).set_id(
      PaintImage::GetNextId());
  auto paint_image = builder.TakePaintImage();
  auto paint_record = sk_make_sp<PaintOpBuffer>();
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(FilterQuality()));
  paint_record->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling, nullptr);
  auto paint_record_shader = PaintShader::MakePaintRecord(
      paint_record, gfx::RectToSkRect(rect), SkTileMode::kRepeat,
      SkTileMode::kRepeat, nullptr);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<ScaleOp>(0.5f, 0.5f);
  PaintFlags raster_flags;
  raster_flags.setShader(paint_record_shader);
  raster_flags.setFilterQuality(FilterQuality());
  display_item_list->push<DrawRectOp>(gfx::RectToSkRect(rect), raster_flags);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, rect.size());
  // Set the shader has animated images so gpu also goes through cc's image
  // upload stack, instead of using skia.
  RasterOptions expected_options(rect.size());
  expected_options.shader_with_animated_images = paint_record_shader.get();
  auto expected = RasterExpectedBitmap(display_item_list, expected_options);
  ExpectEquals(actual, expected);
}

TEST_F(OopImagePixelTest, DrawRecordShaderTranslatedTileRect) {
  auto paint_record = sk_make_sp<PaintOpBuffer>();

  // Arbitrary offsets.  The DrawRectOp inside the PaintShader draws
  // with this offset, but the tile rect also has this offset, so they
  // should cancel out, and it should be as if the DrawRectOp was at the
  // origin.
  int x_offset = 3901;
  int y_offset = -234;

  // Shader here is a tiled 2x3 rectangle with a 1x2 green block in the
  // upper left and a 10pixel wide right/bottom border.  The shader
  // tiling starts from the origin, so starting at 2,1 in the offset_rect
  // below cuts off part of that, leaving two green i's.
  PaintFlags internal_flags;
  internal_flags.setColor(SK_ColorGREEN);
  sk_sp<PaintOpBuffer> shader_buffer(new PaintOpBuffer);
  shader_buffer->push<DrawRectOp>(SkRect::MakeXYWH(x_offset, y_offset, 1, 2),
                                  internal_flags);

  SkRect tile_rect = SkRect::MakeXYWH(x_offset, y_offset, 2, 3);
  sk_sp<PaintShader> paint_record_shader = PaintShader::MakePaintRecord(
      shader_buffer, tile_rect, SkTileMode::kRepeat, SkTileMode::kRepeat,
      nullptr, PaintShader::ScalingBehavior::kRasterAtScale);
  // Force paint_flags to convert this to kFixedScale, so we can safely compare
  // pixels between direct and oop-r modes (since oop will convert to
  // kFixedScale no matter what.
  paint_record_shader->set_has_animated_images(true);

  gfx::Size output_size(10, 10);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawColorOp>(SK_ColorWHITE, SkBlendMode::kSrc);
  display_item_list->push<ScaleOp>(2.f, 2.f);
  PaintFlags raster_flags;
  raster_flags.setShader(paint_record_shader);
  SkRect offset_rect = SkRect::MakeXYWH(2, 1, 10, 10);
  display_item_list->push<DrawRectOp>(offset_rect, raster_flags);
  display_item_list->EndPaintOfUnpaired(gfx::Rect(output_size));
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, output_size);
  auto expected = RasterExpectedBitmap(display_item_list, output_size);
  ExpectEquals(actual, expected);
}

TEST_P(OopImagePixelTest, DrawImageWithTargetColorSpace) {
  SCOPED_TRACE(base::StringPrintf("UseTooLargeImage: %d, FilterQuality: %d\n",
                                  UseTooLargeImage(), FilterQuality()));

  gfx::Rect rect(10, 10);
  gfx::Size image_size = GetImageSize();

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(image_size.width(), image_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorMAGENTA);
  SkPaint green;
  green.setColor(SK_ColorGREEN);
  canvas.drawRect(SkRect::MakeXYWH(1, 2, 3, 4), green);

  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
  const PaintImage::Id kSomeId = 32;
  auto builder =
      PaintImageBuilder::WithDefault().set_image(image, 0).set_id(kSomeId);
  auto paint_image = builder.TakePaintImage();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(FilterQuality()));
  display_item_list->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling,
                                       nullptr);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  RasterOptions options(rect.size());
  options.color_space = gfx::ColorSpace::CreateDisplayP3D65();

  auto actual = Raster(display_item_list, options);
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);

  // Verify some conversion occurred here and that actual != bitmap.
  EXPECT_NE(actual.getColor(0, 0), SK_ColorMAGENTA);
}

TEST_P(OopImagePixelTest, DrawImageWithSourceColorSpace) {
  SCOPED_TRACE(base::StringPrintf("UseTooLargeImage: %d, FilterQuality: %d\n",
                                  UseTooLargeImage(), FilterQuality()));

  gfx::Rect rect(10, 10);
  gfx::Size image_size = GetImageSize();

  auto color_space = gfx::ColorSpace::CreateDisplayP3D65().ToSkColorSpace();
  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(image_size.width(), image_size.height(),
                                 color_space),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorMAGENTA);
  SkPaint green;
  green.setColor(SK_ColorGREEN);
  canvas.drawRect(SkRect::MakeXYWH(1, 2, 3, 4), green);

  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
  const PaintImage::Id kSomeId = 32;
  auto builder =
      PaintImageBuilder::WithDefault().set_image(image, 0).set_id(kSomeId);
  auto paint_image = builder.TakePaintImage();
  EXPECT_EQ(paint_image.color_space(), color_space.get());

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(FilterQuality()));
  display_item_list->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling,
                                       nullptr);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  RasterOptions options(rect.size());

  auto actual = Raster(display_item_list, options);
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);

  // Colors get converted when being drawn to the bitmap.
  EXPECT_NE(bitmap.getColor(0, 0), SK_ColorMAGENTA);
}

TEST_P(OopImagePixelTest, DrawImageWithSourceAndTargetColorSpace) {
  SCOPED_TRACE(base::StringPrintf("UseTooLargeImage: %d, FilterQuality: %d\n",
                                  UseTooLargeImage(), FilterQuality()));

  gfx::Rect rect(10, 10);

  gfx::Size image_size = GetImageSize();
  auto color_space = gfx::ColorSpace::CreateXYZD50().ToSkColorSpace();
  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(image_size.width(), image_size.height(),
                                 color_space),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorMAGENTA);
  SkPaint green;
  green.setColor(SK_ColorGREEN);
  canvas.drawRect(SkRect::MakeXYWH(1, 2, 3, 4), green);

  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
  const PaintImage::Id kSomeId = 32;
  auto builder =
      PaintImageBuilder::WithDefault().set_image(image, 0).set_id(kSomeId);
  auto paint_image = builder.TakePaintImage();
  EXPECT_EQ(paint_image.color_space(), color_space.get());

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(FilterQuality()));
  display_item_list->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling,
                                       nullptr);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  RasterOptions options(rect.size());
  options.color_space = gfx::ColorSpace::CreateDisplayP3D65();

  auto actual = Raster(display_item_list, options);
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);
}

TEST_P(OopImagePixelTest, DrawImageWithSetMatrix) {
  SCOPED_TRACE(base::StringPrintf("UseTooLargeImage: %d, FilterQuality: %d\n",
                                  UseTooLargeImage(), FilterQuality()));

  gfx::Rect rect(10, 10);
  gfx::Size image_size = GetImageSize();

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(image_size.width(), image_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorMAGENTA);
  SkPaint green;
  green.setColor(SK_ColorGREEN);
  canvas.drawRect(SkRect::MakeXYWH(1, 2, 3, 4), green);

  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
  const PaintImage::Id kSomeId = 32;
  auto builder =
      PaintImageBuilder::WithDefault().set_image(image, 0).set_id(kSomeId);
  auto paint_image = builder.TakePaintImage();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  SkSamplingOptions sampling(
      PaintFlags::FilterQualityToSkSamplingOptions(FilterQuality()));
  display_item_list->push<SetMatrixOp>(SkM44::Scale(0.5f, 0.5f));
  display_item_list->push<DrawImageOp>(paint_image, 0.f, 0.f, sampling,
                                       nullptr);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, rect.size());
  auto expected = RasterExpectedBitmap(display_item_list, rect.size());
  ExpectEquals(actual, expected);

  EXPECT_EQ(actual.getColor(0, 0), SK_ColorMAGENTA);
}

namespace {
class TestMailboxBacking : public TextureBacking {
 public:
  explicit TestMailboxBacking(gpu::Mailbox mailbox, SkImageInfo info)
      : mailbox_(mailbox), info_(info) {}

  const SkImageInfo& GetSkImageInfo() override { return info_; }
  gpu::Mailbox GetMailbox() const override { return mailbox_; }
  sk_sp<SkImage> GetAcceleratedSkImage() override { return nullptr; }
  sk_sp<SkImage> GetSkImageViaReadback() override { return nullptr; }
  bool readPixels(const SkImageInfo& dstInfo,
                  void* dstPixels,
                  size_t dstRowBytes,
                  int srcX,
                  int srcY) override {
    return false;
  }
  void FlushPendingSkiaOps() override {}

 private:
  gpu::Mailbox mailbox_;
  SkImageInfo info_;
};
}  // namespace

TEST_F(OopPixelTest, DrawMailboxBackedImage) {
  RasterOptions options(gfx::Size(16, 16));
  options.image_provider_raster_mode = PlaybackImageProvider::RasterMode::kOop;
  SkImageInfo backing_info = SkImageInfo::MakeN32Premul(
      options.resource_size.width(), options.resource_size.height());

  SkBitmap expected_bitmap;
  expected_bitmap.allocPixels(backing_info);

  SkCanvas canvas(expected_bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorMAGENTA);
  SkPaint green;
  green.setColor(SK_ColorGREEN);
  canvas.drawRect(SkRect::MakeXYWH(1, 2, 3, 4), green);

  auto* ri = raster_context_provider_->RasterInterface();
  auto* sii = raster_context_provider_->SharedImageInterface();
  gpu::Mailbox src_mailbox = CreateMailboxSharedImage(
      ri, sii, options, viz::ResourceFormat::RGBA_8888);
  ri->OrderingBarrierCHROMIUM();

  auto* gl = gles2_context_provider_->ContextGL();
  UploadPixels(gl, src_mailbox, options.resource_size, GL_RGBA,
               GL_UNSIGNED_BYTE, expected_bitmap.getPixels());
  gl->OrderingBarrierCHROMIUM();

  auto src_paint_image =
      PaintImageBuilder::WithDefault()
          .set_id(PaintImage::GetNextId())
          .set_texture_backing(sk_sp<TestMailboxBacking>(new TestMailboxBacking(
                                   src_mailbox, backing_info)),
                               PaintImage::GetNextContentId())
          .TakePaintImage();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawImageOp>(src_paint_image, 0.f, 0.f);
  display_item_list->EndPaintOfUnpaired(gfx::Rect(options.resource_size));
  display_item_list->Finalize();

  auto actual_bitmap = Raster(display_item_list, options);
  ExpectEquals(actual_bitmap, expected_bitmap);
}

TEST_F(OopPixelTest, Preclear) {
  gfx::Rect rect(10, 10);
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->Finalize();

  RasterOptions options;
  options.resource_size = rect.size();
  options.full_raster_rect = rect;
  options.playback_rect = rect;
  options.background_color = SK_ColorMAGENTA;
  options.preclear = true;
  options.preclear_color = SK_ColorGREEN;

  auto actual = Raster(display_item_list, options);

  options.preclear = false;
  options.background_color = SK_ColorGREEN;
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);
}

TEST_P(OopClearPixelTest, ClearingOpaqueCorner) {
  // Verify that clears work properly for both the right and bottom sides
  // of an opaque corner tile.

  RasterOptions options;
  gfx::Point arbitrary_offset(10, 20);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(8, 7));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  if (IsPartialRaster()) {
    options.playback_rect = gfx::Rect(options.full_raster_rect.x() + 1,
                                      options.full_raster_rect.y() + 1,
                                      options.full_raster_rect.width() - 1,
                                      options.full_raster_rect.height() - 1);
  } else {
    options.playback_rect = options.full_raster_rect;
  }
  options.background_color = SK_ColorGREEN;
  float arbitrary_scale = 0.25f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);
  if (IsPartialRaster()) {
    // Expect a two pixel border from texels 7-9 on the column and 6-8 on row,
    // ignoring the top row and left column.
    canvas.drawRect(SkRect::MakeXYWH(7, 1, 2, 7), green);
    canvas.drawRect(SkRect::MakeXYWH(1, 6, 8, 2), green);
  } else {
    // Expect a two pixel border from texels 7-9 on the column and 6-8 on row.
    canvas.drawRect(SkRect::MakeXYWH(7, 0, 2, 8), green);
    canvas.drawRect(SkRect::MakeXYWH(0, 6, 9, 2), green);
  }

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingOpaqueCornerExactEdge) {
  // Verify that clears work properly for both the right and bottom sides
  // of an opaque corner tile whose content rect exactly lines up with
  // the edge of the resource.

  RasterOptions options;
  gfx::Point arbitrary_offset(10, 20);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, options.resource_size);
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect = options.full_raster_rect;
  options.background_color = SK_ColorGREEN;
  float arbitrary_scale = 0.25f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Expect a one pixel border on the bottom/right edge.
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);
  canvas.drawRect(SkRect::MakeXYWH(9, 0, 1, 10), green);
  canvas.drawRect(SkRect::MakeXYWH(0, 9, 10, 1), green);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingOpaqueCornerPartialRaster) {
  // Verify that clears do nothing on an opaque corner tile whose
  // partial raster rect doesn't intersect the edge of the content.

  RasterOptions options;
  options.resource_size = gfx::Size(10, 10);
  gfx::Point arbitrary_offset(30, 12);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(8, 7));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect =
      gfx::Rect(arbitrary_offset.x() + 5, arbitrary_offset.y() + 3, 2, 3);
  options.background_color = SK_ColorGREEN;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Verify this is internal.
  EXPECT_NE(options.playback_rect.right(), options.full_raster_rect.right());
  EXPECT_NE(options.playback_rect.bottom(), options.full_raster_rect.bottom());

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Expect no clearing here because the playback rect is internal.
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_P(OopClearPixelTest, ClearingOpaqueLeftEdge) {
  // Verify that a tile that intersects the left edge of content
  // but not other edges only clears the left pixels.
  RasterOptions options;
  options.resource_size = gfx::Size(10, 10);
  int arbitrary_y = 10;
  options.full_raster_rect = gfx::Rect(0, arbitrary_y, 3, 10);
  options.content_size = gfx::Size(options.full_raster_rect.right() + 1000,
                                   options.full_raster_rect.bottom() + 1000);
  if (IsPartialRaster()) {
    // Ignore the right column of pixels here to force partial raster.
    // Additionally ignore the top and bottom rows of pixels to make sure
    // that things are not cleared outside the rect.
    options.playback_rect = gfx::Rect(options.full_raster_rect.x(),
                                      options.full_raster_rect.y() + 1,
                                      options.full_raster_rect.width() - 1,
                                      options.full_raster_rect.height() - 2);
  } else {
    options.playback_rect = options.full_raster_rect;
  }

  options.background_color = SK_ColorGREEN;
  options.post_translate = gfx::Vector2dF(0.3f, 0.7f);
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);
  if (IsPartialRaster()) {
    // Expect a one pixel column border on the first column, ignoring the first
    // and the last rows.
    canvas.drawRect(SkRect::MakeXYWH(0, 1, 1, 8), green);
  } else {
    // Expect a one pixel column border on the first column.
    canvas.drawRect(SkRect::MakeXYWH(0, 0, 1, 10), green);
  }

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_P(OopClearPixelTest, ClearingOpaqueRightEdge) {
  // Verify that a tile that intersects the right edge of content
  // but not other edges only clears the right pixels.
  RasterOptions options;
  gfx::Point arbitrary_offset(30, 40);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(3, 10));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom() + 1000);
  if (IsPartialRaster()) {
    // Ignore the left column of pixels here to force partial raster.
    // Additionally ignore the top and bottom rows of pixels to make sure
    // that things are not cleared outside the rect.
    options.playback_rect = gfx::Rect(options.full_raster_rect.x() + 1,
                                      options.full_raster_rect.y() + 1,
                                      options.full_raster_rect.width() - 1,
                                      options.full_raster_rect.height() - 2);
  } else {
    options.playback_rect = options.full_raster_rect;
  }

  options.background_color = SK_ColorGREEN;
  float arbitrary_scale = 0.25f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);
  if (IsPartialRaster()) {
    // Expect a two pixel column border from texels 2-4, ignoring the first and
    // the last rows.
    canvas.drawRect(SkRect::MakeXYWH(2, 1, 2, 8), green);
  } else {
    // Expect a two pixel column border from texels 2-4.
    canvas.drawRect(SkRect::MakeXYWH(2, 0, 2, 10), green);
  }

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_P(OopClearPixelTest, ClearingOpaqueTopEdge) {
  // Verify that a tile that intersects only the top edge of content
  // but not other edges only clears the top pixels.

  RasterOptions options;
  options.resource_size = gfx::Size(10, 10);
  int arbitrary_x = 10;
  options.full_raster_rect = gfx::Rect(arbitrary_x, 0, 10, 5);
  options.content_size = gfx::Size(options.full_raster_rect.right() + 1000,
                                   options.full_raster_rect.bottom() + 1000);
  if (IsPartialRaster()) {
    // Ignore the bottom row of pixels here to force partial raster.
    // Additionally ignore the left and right columns of pixels to make sure
    // that things are not cleared outside the rect.
    options.playback_rect = gfx::Rect(options.full_raster_rect.x() + 1,
                                      options.full_raster_rect.y(),
                                      options.full_raster_rect.width() - 2,
                                      options.full_raster_rect.height() - 1);
  } else {
    options.playback_rect = options.full_raster_rect;
  }
  options.background_color = SK_ColorGREEN;
  options.post_translate = gfx::Vector2dF(0.3f, 0.7f);
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);

  if (IsPartialRaster()) {
    // Expect a one pixel border on the top row, ignoring the first and the last
    // columns.
    canvas.drawRect(SkRect::MakeXYWH(1, 0, 8, 1), green);
  } else {
    // Expect a one pixel border on the top row.
    canvas.drawRect(SkRect::MakeXYWH(0, 0, 10, 1), green);
  }

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_P(OopClearPixelTest, ClearingOpaqueBottomEdge) {
  // Verify that a tile that intersects the bottom edge of content
  // but not other edges only clears the bottom pixels.

  RasterOptions options;
  gfx::Point arbitrary_offset(10, 20);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(10, 5));
  options.content_size = gfx::Size(options.full_raster_rect.right() + 1000,
                                   options.full_raster_rect.bottom());
  if (IsPartialRaster()) {
    // Ignore the top row of pixels here to force partial raster.
    // Additionally ignore the left and right columns of pixels to make sure
    // that things are not cleared outside the rect.
    options.playback_rect = gfx::Rect(options.full_raster_rect.x() + 1,
                                      options.full_raster_rect.y() + 1,
                                      options.full_raster_rect.width() - 2,
                                      options.full_raster_rect.height() - 1);
  } else {
    options.playback_rect = options.full_raster_rect;
  }
  options.background_color = SK_ColorGREEN;
  float arbitrary_scale = 0.25f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);

  if (IsPartialRaster()) {
    // Expect a two pixel border from texels 4-6 on the row, ignoring the first
    // and the last columns.
    canvas.drawRect(SkRect::MakeXYWH(1, 4, 8, 2), green);
  } else {
    // Expect a two pixel border from texels 4-6 on the row
    canvas.drawRect(SkRect::MakeXYWH(0, 4, 10, 2), green);
  }

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingOpaqueInternal) {
  // Verify that an internal opaque tile does no clearing.

  RasterOptions options;
  gfx::Point arbitrary_offset(35, 12);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, options.resource_size);
  // Very large content rect to make this an internal tile.
  options.content_size = gfx::Size(1000, 1000);
  options.playback_rect = options.full_raster_rect;
  options.background_color = SK_ColorGREEN;
  options.post_translate = gfx::Vector2dF(0.3f, 0.7f);
  float arbitrary_scale = 1.2345f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Expect no clears here, as this tile does not intersect the edge of the
  // tile.
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingTransparentCorner) {
  RasterOptions options;
  gfx::Point arbitrary_offset(5, 8);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(8, 7));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect = options.full_raster_rect;
  options.background_color = SK_ColorTRANSPARENT;
  float arbitrary_scale = 3.7f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = true;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  // Because this is rastering the entire tile, clear the entire thing
  // even if the full raster rect doesn't cover the whole resource.
  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorTRANSPARENT);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingTransparentInternalTile) {
  // Content rect much larger than full raster rect or playback rect.
  // This should still clear the tile.
  RasterOptions options;
  gfx::Point arbitrary_offset(100, 200);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, options.resource_size);
  options.content_size = gfx::Size(1000, 1000);
  options.playback_rect = options.full_raster_rect;
  options.background_color = SK_ColorTRANSPARENT;
  float arbitrary_scale = 3.7f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = true;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Note that clearing of the tile should supersede any early outs due to an
  // empty display list. This is due to the fact that partial raster may in fact
  // result in no items being generated, in which case a clear should still
  // happen. See crbug.com/901897.
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  // Because this is rastering the entire tile, clear the entire thing
  // even if the full raster rect doesn't cover the whole resource.
  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorTRANSPARENT);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingTransparentCornerPartialRaster) {
  RasterOptions options;
  options.resource_size = gfx::Size(10, 10);
  gfx::Point arbitrary_offset(30, 12);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(8, 7));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect =
      gfx::Rect(arbitrary_offset.x() + 5, arbitrary_offset.y() + 3, 2, 4);
  options.background_color = SK_ColorTRANSPARENT;
  float arbitrary_scale = 0.23f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = true;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Result should be a red background with a cleared hole where the
  // playback_rect is.
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(options.preclear_color);
  canvas.translate(-arbitrary_offset.x(), -arbitrary_offset.y());
  canvas.clipRect(gfx::RectToSkRect(options.playback_rect));
  canvas.drawColor(SK_ColorTRANSPARENT, SkBlendMode::kSrc);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

// Test various bitmap and playback rects in the raster options, to verify
// that in process (RasterSource) and out of process (GLES2Implementation)
// raster behave identically.
TEST_F(OopPixelTest, DrawRectBasicRasterOptions) {
  PaintFlags flags;
  flags.setColor(SkColorSetARGB(255, 250, 10, 20));
  gfx::Rect draw_rect(3, 1, 8, 9);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawRectOp>(gfx::RectToSkRect(draw_rect), flags);
  display_item_list->EndPaintOfUnpaired(draw_rect);
  display_item_list->Finalize();

  std::vector<std::pair<gfx::Rect, gfx::Rect>> input = {
      {{0, 0, 10, 10}, {0, 0, 10, 10}},
      {{1, 2, 10, 10}, {4, 2, 5, 6}},
      {{5, 5, 15, 10}, {5, 5, 10, 10}}};

  for (size_t i = 0; i < input.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Case %zd", i));

    RasterOptions options;
    options.resource_size = input[i].first.size(),
    options.full_raster_rect = input[i].first;
    options.content_size = gfx::Size(options.full_raster_rect.right(),
                                     options.full_raster_rect.bottom());
    options.playback_rect = input[i].second;
    options.background_color = SK_ColorMAGENTA;

    auto actual = Raster(display_item_list, options);
    auto expected = RasterExpectedBitmap(display_item_list, options);
    ExpectEquals(actual, expected);
  }
}

TEST_F(OopPixelTest, DrawRectScaleTransformOptions) {
  PaintFlags flags;
  // Use powers of two here to make floating point blending consistent.
  flags.setColor(SkColorSetARGB(128, 64, 128, 32));
  flags.setAntiAlias(true);
  gfx::Rect draw_rect(3, 4, 8, 9);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawRectOp>(gfx::RectToSkRect(draw_rect), flags);
  display_item_list->EndPaintOfUnpaired(draw_rect);
  display_item_list->Finalize();

  // Draw a greenish transparent box, partially offset and clipped in the
  // bottom right.  It should appear near the upper left of a cyan background,
  // with the left and top sides of the greenish box partially blended due to
  // the post translate.
  RasterOptions options;
  options.resource_size = {20, 20};
  options.content_size = {25, 25};
  options.full_raster_rect = {5, 5, 20, 20};
  options.playback_rect = {5, 5, 13, 9};
  options.background_color = SK_ColorCYAN;
  options.post_translate = {0.5f, 0.25f};
  options.post_scale = 2.f;

  auto actual = Raster(display_item_list, options);
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, DrawRectTransformOptionsFullRaster) {
  PaintFlags flags;
  // Use powers of two here to make floating point blending consistent.
  flags.setColor(SkColorSetRGB(64, 128, 32));
  flags.setAntiAlias(true);
  gfx::Rect draw_rect(0, 0, 19, 19);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawRectOp>(gfx::RectToSkRect(draw_rect), flags);
  display_item_list->EndPaintOfUnpaired(draw_rect);
  display_item_list->Finalize();

  // The opaque rect above is 1px smaller than the canvas. With the subpixel
  // translation, the rect fills the whole canvas, but the pixels at the edges
  // are translucent. We should clear the canvas before drawing the rect, so
  // the translucent pixels at the edges should not expose the preclear color,
  // even if requires_clear is not true.
  RasterOptions options;
  options.resource_size = {20, 20};
  options.content_size = {25, 25};
  options.full_raster_rect = {5, 5, 20, 20};
  options.playback_rect = {5, 5, 20, 20};
  options.preclear = true;
  options.preclear_color = SK_ColorRED;
  options.post_translate = {0.5f, 0.25f};
  options.post_scale = 2.f;

  auto actual = Raster(display_item_list, options);
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, DrawRectQueryMiddleOfDisplayList) {
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  std::vector<SkColor> colors = {
      SkColorSetARGB(255, 0, 0, 255),    SkColorSetARGB(255, 0, 255, 0),
      SkColorSetARGB(255, 0, 255, 255),  SkColorSetARGB(255, 255, 0, 0),
      SkColorSetARGB(255, 255, 0, 255),  SkColorSetARGB(255, 255, 255, 0),
      SkColorSetARGB(255, 255, 255, 255)};

  for (int i = 0; i < 20; ++i) {
    gfx::Rect draw_rect(0, i, 1, 1);
    PaintFlags flags;
    flags.setColor(colors[i % colors.size()]);
    display_item_list->StartPaint();
    display_item_list->push<DrawRectOp>(gfx::RectToSkRect(draw_rect), flags);
    display_item_list->EndPaintOfUnpaired(draw_rect);
  }
  display_item_list->Finalize();

  // Draw a "tile" in the middle of the display list with a post scale.
  RasterOptions options;
  options.resource_size = {10, 10};
  options.content_size = {20, 20};
  options.full_raster_rect = {0, 10, 1, 10};
  options.playback_rect = {0, 10, 1, 10};
  options.background_color = SK_ColorGRAY;
  options.post_translate = {0.f, 0.f};
  options.post_scale = 2.f;

  auto actual = Raster(display_item_list, options);
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, DrawRectColorSpace) {
  RasterOptions options;
  options.resource_size = gfx::Size(100, 100);
  options.content_size = options.resource_size;
  options.full_raster_rect = gfx::Rect(options.content_size);
  options.playback_rect = options.full_raster_rect;
  options.color_space = gfx::ColorSpace::CreateDisplayP3D65();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  PaintFlags flags;
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(SK_ColorGREEN);
  display_item_list->push<DrawRectOp>(
      gfx::RectToSkRect(gfx::Rect(options.resource_size)), flags);
  display_item_list->EndPaintOfUnpaired(options.full_raster_rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, options);
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);
}

sk_sp<SkTextBlob> BuildTextBlob(
    sk_sp<SkTypeface> typeface = SkTypeface::MakeDefault(),
    bool use_lcd_text = false) {
  if (!typeface) {
    typeface = SkTypeface::MakeFromName("monospace", SkFontStyle());
  }

  SkFont font;
  font.setTypeface(typeface);
  font.setHinting(SkFontHinting::kNormal);
  font.setSize(8.f);
  if (use_lcd_text) {
    font.setSubpixel(true);
    font.setEdging(SkFont::Edging::kSubpixelAntiAlias);
  }

  return SkTextBlob::MakeFromString("Hamburgefons", font);
}

// A reasonable Y offset given the font parameters of BuildTextBlob() that
// ensures the text is not just drawn above the top edge of the surface.
static constexpr SkScalar kTextBlobY = 16.f;

// OopTextBlobPixelTest's test suite runs through the cross product of these
// strategies.
enum class TextBlobStrategy {
  kDirect,        // DrawTextBlobOp directly in the display list
  kDrawRecord,    // DrawRecordOp where the paint record includes text
  kRecordShader,  // DrawRectOp where the paint has a RecordShader with text
  kRecordFilter   // DrawRectOp where the paint has a RecordFilter with text
};
enum class FilterStrategy {
  kNone,        // No additional PaintFilter interacting with text
  kPaintFlags,  // A blur is added to the PaintFlags of the draw
  kSaveLayer    // An explicit save layer with blur is made before the draw
};
enum class MatrixStrategy {
  kIdentity,     // Identity matrix (no extra scale factor for text then)
  kScaled,       // Matrix is an axis-aligned scale factor
  kComplex,      // Matrix is not axis-aligned and scale must be decomposed
  kPerspective,  // Matrix has perspective and an approximate scale is needed
};
enum class LCDStrategy { kNo, kYes };

using TextBlobTestConfig = ::testing::
    tuple<TextBlobStrategy, FilterStrategy, MatrixStrategy, LCDStrategy>;

class OopTextBlobPixelTest
    : public OopPixelTest,
      public ::testing::WithParamInterface<TextBlobTestConfig> {
 public:
  void RunTest() {
    RasterOptions options;
    options.resource_size = gfx::Size(100, 100);
    options.content_size = options.resource_size;
    options.full_raster_rect = gfx::Rect(options.content_size);
    options.playback_rect = options.full_raster_rect;
    options.color_space = gfx::ColorSpace::CreateSRGB();
    options.use_lcd_text = UseLcdText();

    auto display_item_list = base::MakeRefCounted<DisplayItemList>();
    display_item_list->StartPaint();

    // Set matrix before any image filter is applied, which may force the
    // matrix to be decomposed into a transform compatible with the filter.
    SetMatrix(display_item_list);

    const bool save_layer =
        GetFilterStrategy(GetParam()) == FilterStrategy::kSaveLayer;
    sk_sp<PaintFilter> filter = MakeFilter();
    if (save_layer) {
      PaintFlags layer_flags;
      layer_flags.setImageFilter(std::move(filter));
      filter = nullptr;
      display_item_list->push<SaveLayerOp>(nullptr, &layer_flags);
    }

    PushDrawOp(display_item_list, std::move(filter));

    if (save_layer) {
      display_item_list->push<RestoreOp>();
    }

    display_item_list->EndPaintOfUnpaired(options.full_raster_rect);
    display_item_list->Finalize();

    auto actual = Raster(display_item_list, options);
    auto expected = RasterExpectedBitmap(display_item_list, options);

    // Drawing text into an image and then transforming that can lead to small
    // flakiness in devices, although in practice they are very imperceptible,
    // and distinctly different from using the wrong glyph or text params.
    float error_pixels_percentage = 0.f;
    int max_abs_error = 0;
#if defined(OS_ANDROID)
    // The nexus5 and nexus5x bots are particularly susceptible to small changes
    // when bilerping an image (not visible).
    const int sdk = base::android::BuildInfo::GetInstance()->sdk_int();
    if (sdk <= base::android::SDK_VERSION_MARSHMALLOW) {
      error_pixels_percentage = 10.f;
      max_abs_error = 20;
    } else {
      // Newer OSes occasionally have smaller flakes when using the real GPU
      error_pixels_percentage = 1.5f;
      max_abs_error = 2;
    }
#endif
    // Many platforms need very small tolerances under complex transforms,
    // and higher tolerances for perspective, since it triggers path rendering
    // for each glyph. Additionally, record filters require higher tolerance
    // because oop-r converts raster-at-scale to fixed-scale.
    float avg_error = max_abs_error;
    const bool is_record_filter =
        GetTextBlobStrategy(GetParam()) == TextBlobStrategy::kRecordFilter;
    if (GetMatrixStrategy(GetParam()) == MatrixStrategy::kComplex) {
      error_pixels_percentage =
          std::max(is_record_filter ? 12.f : 0.2f, error_pixels_percentage);
      max_abs_error = std::max(is_record_filter ? 220 : 2, max_abs_error);
      avg_error = std::max(is_record_filter ? 50.f : 2.f, avg_error);
    } else if (GetMatrixStrategy(GetParam()) == MatrixStrategy::kPerspective) {
      error_pixels_percentage =
          std::max(is_record_filter ? 13.f : 4.f, error_pixels_percentage);
      max_abs_error = std::max(is_record_filter ? 255 : 36, max_abs_error);
      avg_error = std::max(is_record_filter ? 60.f : 36.f, avg_error);
    }

    FuzzyPixelComparator comparator(
        /*discard_alpha=*/false,
        /*error_pixels_percentage_limit=*/error_pixels_percentage,
        /*small_error_pixels_percentage_limit=*/0.0f,
        /*avg_abs_error_limit=*/avg_error,
        /*max_abs_error_limit=*/max_abs_error,
        /*small_error_threshold=*/0);
    ExpectEquals(actual, expected, comparator);
  }

  sk_sp<PaintFilter> MakeFilter() {
    if (GetFilterStrategy(GetParam()) == FilterStrategy::kNone) {
      return nullptr;
    } else {
      // Keep the blur sigmas small to reduce test duration, it's the presence
      // of the blur filter that triggers the code path changes we care about.
      return sk_make_sp<BlurPaintFilter>(.1f, .1f, SkTileMode::kDecal, nullptr);
    }
  }

  void SetMatrix(scoped_refptr<DisplayItemList> display_list) {
    MatrixStrategy strategy = GetMatrixStrategy(GetParam());

    SkM44 m;  // Default constructed to identity
    if (strategy != MatrixStrategy::kIdentity) {
      // Scaled, Complex, and Perspective all have a 2x scale factor
      m.preScale(2.0f, 2.0f);
      if (strategy == MatrixStrategy::kComplex) {
        SkM44 skew = SkM44();
        skew.setRC(0, 1, 2.f);
        skew.setRC(1, 0, 2.f);
        m.preConcat(skew);
      } else if (strategy == MatrixStrategy::kPerspective) {
        SkM44 persp = SkM44::Perspective(0.01f, 10.f, SK_ScalarPI / 3.f);
        persp.preTranslate(0.f, 5.f, -0.1f);
        persp.preConcat(SkM44::Rotate({0.f, 1.f, 0.f}, 0.008f /* radians */));
        m.postConcat(persp);
      }
    }

    display_list->push<ConcatOp>(m);
  }

  void PushDrawOp(scoped_refptr<DisplayItemList> display_list,
                  sk_sp<PaintFilter> filter) {
    TextBlobStrategy strategy = GetTextBlobStrategy(GetParam());

    auto text_blob = BuildTextBlob(SkTypeface::MakeDefault(), UseLcdText());

    PaintFlags text_flags;
    text_flags.setStyle(PaintFlags::kFill_Style);
    text_flags.setColor(SK_ColorGREEN);
    if (filter && (strategy == TextBlobStrategy::kDirect ||
                   strategy == TextBlobStrategy::kDrawRecord)) {
      // If there's a filter, the only PaintFlags that are available for these
      // two text-drawing strategies is 'text_flags'.
      text_flags.setImageFilter(std::move(filter));
      filter = nullptr;
    }
    if (strategy == TextBlobStrategy::kDirect) {
      display_list->push<DrawTextBlobOp>(std::move(text_blob), 0.0f, kTextBlobY,
                                         text_flags);
      return;
    }

    // All remaining strategies add the DrawTextBlobOp to an inner paint record.
    auto paint_record = sk_make_sp<PaintOpBuffer>();
    paint_record->push<DrawTextBlobOp>(std::move(text_blob), 0.0f, kTextBlobY,
                                       text_flags);
    if (strategy == TextBlobStrategy::kDrawRecord) {
      display_list->push<DrawRecordOp>(std::move(paint_record));
      return;
    }

    PaintFlags record_flags;
    if (strategy == TextBlobStrategy::kRecordShader) {
      auto paint_record_shader = PaintShader::MakePaintRecord(
          paint_record, SkRect::MakeWH(25, 25), SkTileMode::kRepeat,
          SkTileMode::kRepeat, nullptr,
          PaintShader::ScalingBehavior::kRasterAtScale);
      // Force paint_flags to convert this to kFixedScale, so we can safely
      // compare pixels between direct and oop-r modes (since oop will convert
      // to kFixedScale no matter what.
      paint_record_shader->set_has_animated_images(true);

      record_flags.setShader(paint_record_shader);
      record_flags.setImageFilter(std::move(filter));
    } else {
      DCHECK(strategy == TextBlobStrategy::kRecordFilter);

      sk_sp<PaintFilter> paint_record_filter =
          sk_make_sp<RecordPaintFilter>(paint_record, SkRect::MakeWH(100, 100));
      // If there's an additional filter, we have to compose it with the
      // paint record filter.
      if (filter) {
        paint_record_filter = sk_make_sp<ComposePaintFilter>(
            std::move(filter), std::move(paint_record_filter));
      }
      record_flags.setImageFilter(std::move(paint_record_filter));
    }

    // Use bilerp sampling with the PaintRecord to help reduce max RGB error
    // from pixel-snapping flakiness when using NN sampling.
    record_flags.setFilterQuality(PaintFlags::FilterQuality::kLow);

    // The text blob is embedded in a paint record, which is attached to the
    // paint via a shader or image filter. Just draw a rect with the paint.
    display_list->push<DrawRectOp>(SkRect::MakeWH(50, 50), record_flags);
  }

  static TextBlobStrategy GetTextBlobStrategy(
      const TextBlobTestConfig& config) {
    return ::testing::get<0>(config);
  }
  static FilterStrategy GetFilterStrategy(const TextBlobTestConfig& config) {
    return ::testing::get<1>(config);
  }
  static MatrixStrategy GetMatrixStrategy(const TextBlobTestConfig& config) {
    return ::testing::get<2>(config);
  }
  static LCDStrategy GetLCDStrategy(const TextBlobTestConfig& config) {
    return ::testing::get<3>(config);
  }

  bool UseLcdText() const {
    return GetLCDStrategy(GetParam()) == LCDStrategy::kYes;
  }

  static std::string PrintTestName(
      const ::testing::TestParamInfo<TextBlobTestConfig>& info) {
    std::stringstream ss;
    switch (GetTextBlobStrategy(info.param)) {
      case TextBlobStrategy::kDirect:
        ss << "Direct";
        break;
      case TextBlobStrategy::kDrawRecord:
        ss << "DrawRecord";
        break;
      case TextBlobStrategy::kRecordShader:
        ss << "RecordShader";
        break;
      case TextBlobStrategy::kRecordFilter:
        ss << "RecordFilter";
        break;
    }
    ss << "_";
    switch (GetFilterStrategy(info.param)) {
      case FilterStrategy::kNone:
        ss << "NoFilter";
        break;
      case FilterStrategy::kPaintFlags:
        ss << "FilterOnPaint";
        break;
      case FilterStrategy::kSaveLayer:
        ss << "FilterOnLayer";
        break;
    }
    ss << "_";
    switch (GetMatrixStrategy(info.param)) {
      case MatrixStrategy::kIdentity:
        ss << "IdentityCTM";
        break;
      case MatrixStrategy::kScaled:
        ss << "ScaledCTM";
        break;
      case MatrixStrategy::kComplex:
        ss << "ComplexCTM";
        break;
      case MatrixStrategy::kPerspective:
        ss << "PerspectiveCTM";
        break;
    }
    ss << "_";
    switch (GetLCDStrategy(info.param)) {
      case LCDStrategy::kNo:
        ss << "NoLCD";
        break;
      case LCDStrategy::kYes:
        ss << "LCD";
        break;
    }

    return ss.str();
  }
};

TEST_P(OopTextBlobPixelTest, Config) {
  RunTest();
}

INSTANTIATE_TEST_SUITE_P(
    P,
    OopTextBlobPixelTest,
    ::testing::Combine(::testing::Values(TextBlobStrategy::kDirect,
                                         TextBlobStrategy::kDrawRecord,
                                         TextBlobStrategy::kRecordShader,
                                         TextBlobStrategy::kRecordFilter),
                       ::testing::Values(FilterStrategy::kNone,
                                         FilterStrategy::kPaintFlags,
                                         FilterStrategy::kSaveLayer),
                       ::testing::Values(MatrixStrategy::kIdentity,
                                         MatrixStrategy::kScaled,
                                         MatrixStrategy::kComplex,
                                         MatrixStrategy::kPerspective),
                       ::testing::Values(LCDStrategy::kNo, LCDStrategy::kYes)),
    OopTextBlobPixelTest::PrintTestName);

void ClearFontCache(CompletionEvent* event) {
  SkGraphics::PurgeFontCache();
  event->Signal();
}

TEST_F(OopPixelTest, DrawTextMultipleRasterCHROMIUM) {
  RasterOptions options;
  options.resource_size = gfx::Size(100, 100);
  options.content_size = options.resource_size;
  options.full_raster_rect = gfx::Rect(options.content_size);
  options.playback_rect = options.full_raster_rect;
  options.color_space = gfx::ColorSpace::CreateSRGB();

  auto sk_typeface_1 = SkTypeface::MakeFromName("monospace", SkFontStyle());
  auto sk_typeface_2 = SkTypeface::MakeFromName("roboto", SkFontStyle());

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  PaintFlags flags;
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(SK_ColorGREEN);
  display_item_list->push<DrawTextBlobOp>(BuildTextBlob(sk_typeface_1), 0.0f,
                                          kTextBlobY, flags);
  display_item_list->EndPaintOfUnpaired(options.full_raster_rect);
  display_item_list->Finalize();

  // Create another list with a different typeface.
  auto display_item_list_2 = base::MakeRefCounted<DisplayItemList>();
  display_item_list_2->StartPaint();
  display_item_list_2->push<DrawTextBlobOp>(BuildTextBlob(sk_typeface_2), 0.0f,
                                            kTextBlobY, flags);
  display_item_list_2->EndPaintOfUnpaired(options.full_raster_rect);
  display_item_list_2->Finalize();

  // Raster both these lists with 2 RasterCHROMIUM commands between a single
  // Begin/EndRaster sequence.
  options.additional_lists = {display_item_list_2};
  Raster(display_item_list, options);

  // Clear skia's font cache. No entries should remain since the service
  // should unpin everything.
  EXPECT_GT(SkGraphics::GetFontCacheUsed(), 0u);
  CompletionEvent event;
  raster_context_provider_->ExecuteOnGpuThread(
      base::BindOnce(&ClearFontCache, &event));
  event.Wait();
  EXPECT_EQ(SkGraphics::GetFontCacheUsed(), 0u);
}

TEST_F(OopPixelTest, DrawTextBlobPersistentShaderCache) {
  RasterOptions options;
  options.resource_size = gfx::Size(100, 100);
  options.content_size = options.resource_size;
  options.full_raster_rect = gfx::Rect(options.content_size);
  options.playback_rect = options.full_raster_rect;
  options.color_space = gfx::ColorSpace::CreateSRGB();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  PaintFlags flags;
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(SK_ColorGREEN);
  display_item_list->push<DrawTextBlobOp>(BuildTextBlob(), 0.0f, kTextBlobY,
                                          flags);
  display_item_list->EndPaintOfUnpaired(options.full_raster_rect);
  display_item_list->Finalize();

  auto expected = RasterExpectedBitmap(display_item_list, options);
  auto actual = Raster(display_item_list, options);
  ExpectEquals(actual, expected);

  // Re-create the context so we start with an uninitialized skia memory cache
  // and use shaders from the persistent cache.
  InitializeOOPContext();
  actual = Raster(display_item_list, options);
  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, WritePixels) {
  gfx::Size dest_size(10, 10);
  RasterOptions options(dest_size);
  auto* ri = raster_context_provider_->RasterInterface();
  auto* sii = raster_context_provider_->SharedImageInterface();
  gpu::Mailbox dest_mailbox = CreateMailboxSharedImage(
      ri, sii, options, viz::ResourceFormat::RGBA_8888);
  std::vector<SkPMColor> expected_pixels(dest_size.width() * dest_size.height(),
                                         SkPreMultiplyARGB(255, 0, 0, 255));
  SkBitmap expected;
  expected.installPixels(
      SkImageInfo::MakeN32Premul(dest_size.width(), dest_size.height()),
      expected_pixels.data(), dest_size.width() * sizeof(SkColor));

  ri->WritePixels(dest_mailbox, 0, 0, 0, expected.info().minRowBytes(),
                  expected.info(), expected.getPixels());
  ri->OrderingBarrierCHROMIUM();
  EXPECT_EQ(ri->GetError(), static_cast<unsigned>(GL_NO_ERROR));

  gpu::gles2::GLES2Interface* gl = gles2_context_provider_->ContextGL();
  SkBitmap actual = ReadbackMailbox(gl, dest_mailbox, options);
  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  sii->DestroySharedImage(sync_token, dest_mailbox);
  ExpectEquals(actual, expected);
}

namespace {
GrBackendTexture MakeBackendTexture(gpu::gles2::GLES2Interface* gl,
                                    const gpu::Mailbox& mailbox,
                                    gfx::Size size,
                                    GLenum type) {
  GrGLTextureInfo tex_info = {
      GL_TEXTURE_2D, gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name),
      type};
  return GrBackendTexture(size.width(), size.height(), GrMipMapped::kNo,
                          tex_info);
}
}  // namespace

TEST_F(OopPixelTest, ConvertYUVToRGB) {
  RasterOptions options(gfx::Size(16, 16));
  RasterOptions uv_options(gfx::Size(options.resource_size.width() / 2,
                                     options.resource_size.height() / 2));
  auto* ri = raster_context_provider_->RasterInterface();
  auto* sii = raster_context_provider_->SharedImageInterface();
  gpu::Mailbox dest_mailbox = CreateMailboxSharedImage(
      ri, sii, options, viz::ResourceFormat::RGBA_8888);
  gpu::Mailbox yuv_mailboxes[3]{
      CreateMailboxSharedImage(ri, sii, options,
                               viz::ResourceFormat::LUMINANCE_8),
      CreateMailboxSharedImage(ri, sii, uv_options,
                               viz::ResourceFormat::LUMINANCE_8),
      CreateMailboxSharedImage(ri, sii, uv_options,
                               viz::ResourceFormat::LUMINANCE_8)};

  size_t y_pixels_size = options.resource_size.GetArea();
  size_t uv_pixels_size = uv_options.resource_size.GetArea();
  auto y_pix = std::make_unique<uint8_t[]>(y_pixels_size);
  auto u_pix = std::make_unique<uint8_t[]>(uv_pixels_size);
  auto v_pix = std::make_unique<uint8_t[]>(uv_pixels_size);

  // Create a blue image
  memset(y_pix.get(), 0x1d, y_pixels_size);
  memset(u_pix.get(), 0xff, uv_pixels_size);
  memset(v_pix.get(), 0x6b, uv_pixels_size);

  // Upload initial yuv image data
  gpu::gles2::GLES2Interface* gl = gles2_context_provider_->ContextGL();
  UploadPixels(gl, yuv_mailboxes[0], options.resource_size, GL_LUMINANCE,
               GL_UNSIGNED_BYTE, y_pix.get());
  UploadPixels(gl, yuv_mailboxes[1], uv_options.resource_size, GL_LUMINANCE,
               GL_UNSIGNED_BYTE, u_pix.get());
  UploadPixels(gl, yuv_mailboxes[2], uv_options.resource_size, GL_LUMINANCE,
               GL_UNSIGNED_BYTE, v_pix.get());
  gl->OrderingBarrierCHROMIUM();

  ri->ConvertYUVAMailboxesToRGB(dest_mailbox, kJPEG_SkYUVColorSpace,
                                SkYUVAInfo::PlaneConfig::kY_U_V,
                                SkYUVAInfo::Subsampling::k420, yuv_mailboxes);
  ri->OrderingBarrierCHROMIUM();
  SkBitmap actual_bitmap = ReadbackMailbox(gl, dest_mailbox, options);

  // Create the expected result using SkImage::MakeFromYUVTextures
  GrBackendTexture backend_textures[3];
  backend_textures[0] = MakeBackendTexture(
      gl, yuv_mailboxes[0], options.resource_size, GL_LUMINANCE8_EXT);
  backend_textures[1] = MakeBackendTexture(
      gl, yuv_mailboxes[1], uv_options.resource_size, GL_LUMINANCE8_EXT);
  backend_textures[2] = MakeBackendTexture(
      gl, yuv_mailboxes[2], uv_options.resource_size, GL_LUMINANCE8_EXT);

  SkYUVAInfo yuva_info(
      {options.resource_size.width(), options.resource_size.height()},
      SkYUVAInfo::PlaneConfig::kY_U_V, SkYUVAInfo::Subsampling::k420,
      kJPEG_Full_SkYUVColorSpace);
  GrYUVABackendTextures yuva_textures(yuva_info, backend_textures,
                                      kTopLeft_GrSurfaceOrigin);

  auto expected_image = SkImage::MakeFromYUVATextures(
      gles2_context_provider_->GrContext(), yuva_textures);

  SkBitmap expected_bitmap;
  expected_bitmap.allocN32Pixels(options.resource_size.width(),
                                 options.resource_size.height());
  expected_image->readPixels(expected_bitmap.pixmap(), 0, 0);
  ExpectEquals(actual_bitmap, expected_bitmap);

  for (auto& backend : backend_textures) {
    GrGLTextureInfo info;
    if (backend.getGLTextureInfo(&info))
      gl->DeleteTextures(1, &info.fID);
  }

  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  sii->DestroySharedImage(sync_token, dest_mailbox);
  sii->DestroySharedImage(sync_token, yuv_mailboxes[0]);
  sii->DestroySharedImage(sync_token, yuv_mailboxes[1]);
  sii->DestroySharedImage(sync_token, yuv_mailboxes[2]);
}

TEST_F(OopPixelTest, ReadbackImagePixels) {
  RasterOptions options(gfx::Size(16, 16));
  SkImageInfo dest_info = SkImageInfo::MakeN32Premul(
      options.resource_size.width(), options.resource_size.height(),
      gfx::ColorSpace::CreateSRGB().ToSkColorSpace());

  SkBitmap expected_bitmap;
  expected_bitmap.allocPixels(dest_info);

  SkCanvas canvas(expected_bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorMAGENTA);
  SkPaint green;
  green.setColor(SK_ColorGREEN);
  canvas.drawRect(SkRect::MakeXYWH(1, 2, 3, 4), green);

  auto* ri = raster_context_provider_->RasterInterface();
  auto* sii = raster_context_provider_->SharedImageInterface();
  gpu::Mailbox mailbox = CreateMailboxSharedImage(
      ri, sii, options, viz::ResourceFormat::RGBA_8888);
  ri->OrderingBarrierCHROMIUM();

  gpu::gles2::GLES2Interface* gl = gles2_context_provider_->ContextGL();
  UploadPixels(gl, mailbox, options.resource_size, GL_RGBA, GL_UNSIGNED_BYTE,
               expected_bitmap.getPixels());
  gl->OrderingBarrierCHROMIUM();

  SkBitmap actual_bitmap;
  actual_bitmap.allocPixels(dest_info);

  ri->ReadbackImagePixels(mailbox, dest_info, dest_info.minRowBytes(), 0, 0,
                          actual_bitmap.getPixels());
  EXPECT_EQ(ri->GetError(), static_cast<unsigned>(GL_NO_ERROR));
  ri->OrderingBarrierCHROMIUM();

  ExpectEquals(actual_bitmap, expected_bitmap);

  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  sii->DestroySharedImage(sync_token, mailbox);
}

// A workaround on Android that forces the use of GLES 2.0 instead of 3.0
// prevents the use of the GL_RG textures required for NV12 format. This
// test will be reactiviated on Android once the workaround is removed.
#if !defined(OS_ANDROID)
TEST_F(OopPixelTest, ConvertNV12ToRGB) {
  RasterOptions options(gfx::Size(16, 16));
  RasterOptions uv_options(gfx::Size(options.resource_size.width() / 2,
                                     options.resource_size.height() / 2));
  auto* ri = raster_context_provider_->RasterInterface();
  auto* sii = raster_context_provider_->SharedImageInterface();

  gpu::Mailbox dest_mailbox = CreateMailboxSharedImage(
      ri, sii, options, viz::ResourceFormat::RGBA_8888);
  gpu::Mailbox y_uv_mailboxes[2]{
      CreateMailboxSharedImage(ri, sii, options,
                               viz::ResourceFormat::LUMINANCE_8),
      CreateMailboxSharedImage(ri, sii, uv_options, viz::ResourceFormat::RG_88),
  };

  size_t y_pixels_size = options.resource_size.GetArea();
  size_t uv_pixels_size = uv_options.resource_size.GetArea() * 2;
  auto y_pix = std::make_unique<uint8_t[]>(y_pixels_size);
  auto uv_pix = std::make_unique<uint8_t[]>(uv_pixels_size);

  memset(y_pix.get(), 0x1d, y_pixels_size);
  for (size_t i = 0; i < uv_pixels_size; i += 2) {
    uv_pix[i] = 0xff;
    uv_pix[i + 1] = 0x6d;
  }

  gpu::gles2::GLES2Interface* gl = gles2_context_provider_->ContextGL();
  UploadPixels(gl, y_uv_mailboxes[0], options.resource_size, GL_LUMINANCE,
               GL_UNSIGNED_BYTE, y_pix.get());
  UploadPixels(gl, y_uv_mailboxes[1], uv_options.resource_size, GL_RG,
               GL_UNSIGNED_BYTE, uv_pix.get());
  gl->OrderingBarrierCHROMIUM();

  ri->ConvertYUVAMailboxesToRGB(dest_mailbox, kJPEG_SkYUVColorSpace,
                                SkYUVAInfo::PlaneConfig::kY_UV,
                                SkYUVAInfo::Subsampling::k420, y_uv_mailboxes);
  ri->OrderingBarrierCHROMIUM();
  SkBitmap actual_bitmap = ReadbackMailbox(gl, dest_mailbox, options);

  // Create the expected result using SkImage::MakeFromYUVTextures
  GrBackendTexture backend_textures[2];
  backend_textures[0] = MakeBackendTexture(
      gl, y_uv_mailboxes[0], options.resource_size, GL_LUMINANCE8_EXT);
  backend_textures[1] = MakeBackendTexture(gl, y_uv_mailboxes[1],
                                           uv_options.resource_size, GL_RG8);

  SkYUVAInfo yuva_info(
      {options.resource_size.width(), options.resource_size.height()},
      SkYUVAInfo::PlaneConfig::kY_UV, SkYUVAInfo::Subsampling::k420,
      kJPEG_Full_SkYUVColorSpace);
  GrYUVABackendTextures yuva_textures(yuva_info, backend_textures,
                                      kTopLeft_GrSurfaceOrigin);
  auto expected_image = SkImage::MakeFromYUVATextures(
      gles2_context_provider_->GrContext(), yuva_textures);

  SkBitmap expected_bitmap;
  expected_bitmap.allocN32Pixels(options.resource_size.width(),
                                 options.resource_size.height());
  expected_image->readPixels(expected_bitmap.pixmap(), 0, 0);
  ExpectEquals(actual_bitmap, expected_bitmap);

  for (auto& backend : backend_textures) {
    GrGLTextureInfo info;
    if (backend.getGLTextureInfo(&info))
      gl->DeleteTextures(1, &info.fID);
  }

  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  sii->DestroySharedImage(sync_token, dest_mailbox);
  sii->DestroySharedImage(sync_token, y_uv_mailboxes[0]);
  sii->DestroySharedImage(sync_token, y_uv_mailboxes[1]);
}
#endif  // !defined(OS_ANDROID)

class OopPathPixelTest : public OopPixelTest,
                         public ::testing::WithParamInterface<bool> {
 public:
  bool AllowInlining() const { return GetParam(); }
  void RunTest() {
    auto* ri = static_cast<gpu::raster::RasterImplementation*>(
        raster_context_provider_->RasterInterface());
    uint32_t max_inlined_entry_size =
        AllowInlining() ? std::numeric_limits<uint32_t>::max() : 0u;
    ri->set_max_inlined_entry_size_for_testing(max_inlined_entry_size);

    RasterOptions options;
    options.resource_size = gfx::Size(100, 100);
    options.content_size = options.resource_size;
    options.full_raster_rect = gfx::Rect(options.content_size);
    options.playback_rect = options.full_raster_rect;
    options.color_space = gfx::ColorSpace::CreateSRGB();

    auto display_item_list = base::MakeRefCounted<DisplayItemList>();
    display_item_list->StartPaint();
    display_item_list->push<DrawColorOp>(SK_ColorWHITE, SkBlendMode::kSrc);
    PaintFlags flags;
    flags.setStyle(PaintFlags::kFill_Style);
    flags.setColor(SK_ColorGREEN);
    SkPath path;
    path.addCircle(20, 20, 10);
    display_item_list->push<DrawPathOp>(path, flags);
    flags.setColor(SK_ColorBLUE);
    display_item_list->push<DrawRectOp>(SkRect::MakeWH(10, 10), flags);
    display_item_list->EndPaintOfUnpaired(options.full_raster_rect);
    display_item_list->Finalize();

    // Allow 8 pixels in 100x100 image to be different due to non-AA pixel
    // rounding (hence 255 for error limit).
    FuzzyPixelComparator comparator(
        /*discard_alpha=*/false,
        /*error_pixels_percentage_limit=*/0.08f,
        /*small_error_pixels_percentage_limit=*/0.0f,
        /*avg_abs_error_limit=*/255,
        /*max_abs_error_limit=*/255,
        /*small_error_threshold=*/0);
    auto expected = RasterExpectedBitmap(display_item_list, options);
    auto actual = Raster(display_item_list, options);
    ExpectEquals(actual, expected, comparator);
  }
};

TEST_P(OopPathPixelTest, Basic) {
  RunTest();
}

TEST_F(OopPixelTest, RecordShaderExceedsMaxTextureSize) {
  const int max_texture_size =
      raster_context_provider_->ContextCapabilities().max_texture_size;
  const SkRect rect = SkRect::MakeWH(max_texture_size + 10, 10);

  auto shader_record = sk_make_sp<PaintRecord>();
  shader_record->push<DrawColorOp>(SK_ColorWHITE, SkBlendMode::kSrc);
  PaintFlags flags;
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(SK_ColorGREEN);
  shader_record->push<DrawRectOp>(rect, flags);
  auto shader = PaintShader::MakePaintRecord(
      shader_record, rect, SkTileMode::kRepeat, SkTileMode::kRepeat, nullptr);

  RasterOptions options;
  options.resource_size = gfx::Size(100, 100);
  options.content_size = gfx::Size(rect.width(), rect.height());
  options.full_raster_rect = gfx::Rect(options.content_size);
  options.playback_rect = options.full_raster_rect;
  options.color_space = gfx::ColorSpace::CreateSRGB();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawColorOp>(SK_ColorWHITE, SkBlendMode::kSrc);
  flags.setShader(shader);
  display_item_list->push<DrawRectOp>(rect, flags);
  display_item_list->EndPaintOfUnpaired(options.full_raster_rect);
  display_item_list->Finalize();

  auto expected = RasterExpectedBitmap(display_item_list, options);
  auto actual = Raster(display_item_list, options);
  ExpectEquals(actual, expected);
}

INSTANTIATE_TEST_SUITE_P(P, OopImagePixelTest, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(P, OopClearPixelTest, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(P, OopPathPixelTest, ::testing::Bool());

}  // namespace
}  // namespace cc
