// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/solid_color_content_layer_client.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostFiltersPixelTest
    : public LayerTreePixelTest,
      public ::testing::WithParamInterface<LayerTreeTest::RendererType> {
 protected:
  LayerTreeHostFiltersPixelTest() : LayerTreePixelTest(renderer_type()) {}

  RendererType renderer_type() const { return GetParam(); }

  // Text string for graphics backend of the RendererType. Suitable for
  // generating separate base line file paths.
  const char* GetRendererSuffix() {
    switch (renderer_type_) {
      case RENDERER_GL:
        return "gl";
      case RENDERER_SKIA_GL:
        return "skia_gl";
      case RENDERER_SKIA_VK:
        return "skia_vk";
      case RENDERER_SKIA_DAWN:
        return "skia_dawn";
      case RENDERER_SOFTWARE:
        return "sw";
    }
  }

  scoped_refptr<SolidColorLayer> BuildFilterWithGiantCropRect(
      bool masks_to_bounds) {
    scoped_refptr<SolidColorLayer> background =
        CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);
    scoped_refptr<SolidColorLayer> filter_layer =
        CreateSolidColorLayer(gfx::Rect(50, 50, 100, 100), SK_ColorRED);

    // This matrix swaps the red and green channels, and has a slight
    // translation in the alpha component, so that it affects transparent
    // pixels.
    float matrix[20] = {
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 20 / 255.0f,
    };

    FilterOperations filters;
    SkImageFilter::CropRect cropRect(
        SkRect::MakeXYWH(-40000, -40000, 80000, 80000));
    filters.Append(FilterOperation::CreateReferenceFilter(
        sk_make_sp<ColorFilterPaintFilter>(SkColorFilters::Matrix(matrix),
                                           nullptr, &cropRect)));
    filter_layer->SetFilters(filters);
    background->SetMasksToBounds(masks_to_bounds);
    background->AddChild(filter_layer);

    return background;
  }
};

LayerTreeTest::RendererType const kRendererTypes[] = {
    LayerTreeTest::RENDERER_GL,        LayerTreeTest::RENDERER_SKIA_GL,
    LayerTreeTest::RENDERER_SOFTWARE,
#if defined(ENABLE_CC_VULKAN_TESTS)
    LayerTreeTest::RENDERER_SKIA_VK,
#endif  // defined(ENABLE_CC_VULKAN_TESTS)
#if defined(ENABLE_CC_DAWN_TESTS)
    LayerTreeTest::RENDERER_SKIA_DAWN,
#endif  // defined(ENABLE_CC_DAWN_TESTS)
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostFiltersPixelTest,
                         ::testing::ValuesIn(kRendererTypes));

using LayerTreeHostFiltersPixelTestGPU = LayerTreeHostFiltersPixelTest;

LayerTreeTest::RendererType const kRendererTypesGpu[] = {
    LayerTreeTest::RENDERER_GL,
    LayerTreeTest::RENDERER_SKIA_GL,
#if defined(ENABLE_CC_VULKAN_TESTS)
    LayerTreeTest::RENDERER_SKIA_VK,
#endif  // defined(ENABLE_CC_VULKAN_TESTS)
#if defined(ENABLE_CC_DAWN_TESTS)
    LayerTreeTest::RENDERER_SKIA_DAWN,
#endif  // defined(ENABLE_CC_DAWN_TESTS)
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostFiltersPixelTestGPU,
                         ::testing::ValuesIn(kRendererTypesGpu));

TEST_P(LayerTreeHostFiltersPixelTest, BackdropFilterBlurRect) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  // The green box is entirely behind a layer with backdrop blur, so it
  // should appear blurred on its edges.
  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(50, 50, 100, 100), kCSSGreen);
  scoped_refptr<SolidColorLayer> blur = CreateSolidColorLayer(
      gfx::Rect(30, 30, 140, 140), SK_ColorTRANSPARENT);
  background->AddChild(green);
  background->AddChild(blur);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(
      2.f, SkBlurImageFilter::kClamp_TileMode));
  blur->SetBackdropFilters(filters);
  gfx::RRectF backdrop_filter_bounds(gfx::RectF(gfx::SizeF(blur->bounds())), 0);
  blur->SetBackdropFilterBounds(backdrop_filter_bounds);

#if defined(OS_WIN) || defined(ARCH_CPU_ARM64)
  // Windows and ARM64 have 436 pixels off by 1: crbug.com/259915
  float percentage_pixels_large_error = 1.09f;  // 436px / (200*200)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 1.f;
  int large_error_allowed = 1;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error,
      percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels,
      large_error_allowed,
      small_error_allowed));
#endif

  RunPixelTest(
      background,
      use_software_renderer()
          ? base::FilePath(FILE_PATH_LITERAL("backdrop_filter_blur_sw.png"))
          : base::FilePath(FILE_PATH_LITERAL("backdrop_filter_blur.png")));
}

TEST_P(LayerTreeHostFiltersPixelTest, BackdropFilterInvalid) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);
  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(50, 50, 100, 100), kCSSGreen);
  scoped_refptr<SolidColorLayer> blur =
      CreateSolidColorLayer(gfx::Rect(30, 30, 140, 140), SK_ColorTRANSPARENT);
  background->AddChild(green);
  background->AddChild(blur);

  // This should be an invalid filter, and result in just the original green.
  FilterOperations filters;
  filters.Append(FilterOperation::CreateHueRotateFilter(9e99));
  blur->SetBackdropFilters(filters);

  RunPixelTest(
      background,
      base::FilePath(FILE_PATH_LITERAL("backdrop_filter_invalid.png")));
}

TEST_P(LayerTreeHostFiltersPixelTest, BackdropFilterBlurRadius) {
  if (use_software_renderer()) {
    // TODO(989238): Software renderer does not support/implement
    // kClamp_TileMode.
    return;
  }
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(400, 400), SK_ColorRED);
  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(200, 200, 200, 200), kCSSLime);
  gfx::Rect blur_rect(100, 100, 200, 200);
  scoped_refptr<SolidColorLayer> blur =
      CreateSolidColorLayer(blur_rect, SkColorSetARGB(40, 10, 20, 200));
  background->AddChild(green);
  background->AddChild(blur);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(
      30.f, SkBlurImageFilter::kClamp_TileMode));
  blur->SetBackdropFilters(filters);
  gfx::RRectF backdrop_filter_bounds(gfx::RectF(gfx::SizeF(blur->bounds())), 0);
  blur->SetBackdropFilterBounds(backdrop_filter_bounds);

#if defined(OS_WIN) || defined(ARCH_CPU_ARM64)
  // Windows and ARM64 have 436 pixels off by 1: crbug.com/259915
  float percentage_pixels_large_error = 1.09f;  // 436px / (200*200)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 1.f;
  int large_error_allowed = 1;
  int small_error_allowed = 0;
  // Windows using Dawn D3D12 has 2982 pixels off by 1.
  if (use_d3d12())
    percentage_pixels_large_error = 1.864f;  // 2982px / (400*400)
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error, percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels, large_error_allowed,
      small_error_allowed));
#endif
  RunPixelTest(
      background,
      base::FilePath(FILE_PATH_LITERAL("backdrop_filter_blur_radius_.png"))
          .InsertBeforeExtensionASCII(GetRendererSuffix()));
}

TEST_P(LayerTreeHostFiltersPixelTest, BackdropFilterBlurRounded) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  // The green box is entirely behind a layer with backdrop blur, so it
  // should appear blurred on its edges.
  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(50, 50, 100, 100), kCSSGreen);
  scoped_refptr<SolidColorLayer> blur =
      CreateSolidColorLayer(gfx::Rect(46, 46, 108, 108), SK_ColorTRANSPARENT);
  background->AddChild(green);
  background->AddChild(blur);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(
      2.f, SkBlurImageFilter::kClamp_TileMode));
  blur->SetBackdropFilters(filters);
  gfx::RRectF backdrop_filter_bounds(gfx::RectF(gfx::SizeF(blur->bounds())), 14,
                                     16, 18, 20, 22, 30, 40, 50);
  blur->SetBackdropFilterBounds(backdrop_filter_bounds);

  // Skia has various algorithms for clipping by a path, depending on the
  // available hardware, including MSAA techniques. MSAA results vary
  // substantially by platform; with 4x MSAA, a difference of 1 sample can
  // cause up to a 25% color difference!
  // See http://crbug.com/259915
  int small_error_threshold = 64;  // 25% of 255.
  // Allow for ~1 perimeter of the clip path to have a small error.
  float percentage_pixels_small_error = 100.f * (100*4) / (200*200);
  int large_error_limit = 128;  // Off by two samples in 4 MSAA.
  float percentage_pixels_large_or_small_error =
          1.01f * percentage_pixels_small_error;
  // Divide average error by 4 since we blur most of the result.
  float average_error_allowed_in_bad_pixels = small_error_threshold / 4.f;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_or_small_error, percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels, large_error_limit,
      small_error_threshold));

  RunPixelTest(background, use_software_renderer()
                               ? base::FilePath(FILE_PATH_LITERAL(
                                     "backdrop_filter_blur_rounded_sw.png"))
                               : base::FilePath(FILE_PATH_LITERAL(
                                     "backdrop_filter_blur_rounded.png")));
}

TEST_P(LayerTreeHostFiltersPixelTest, BackdropFilterBlurOutsets) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  // The green border is outside the layer with backdrop blur, so the backdrop
  // blur should not include pixels from outside its layer borders. So the
  // interior region of the transparent layer should be perfectly white.
  scoped_refptr<SolidColorLayer> green_border = CreateSolidColorLayerWithBorder(
      gfx::Rect(1, 1, 198, 198), SK_ColorWHITE, 10, kCSSGreen);
  scoped_refptr<SolidColorLayer> blur = CreateSolidColorLayer(
      gfx::Rect(12, 12, 176, 176), SK_ColorTRANSPARENT);
  background->AddChild(green_border);
  background->AddChild(blur);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(
      5.f, SkBlurImageFilter::kClamp_TileMode));
  blur->SetBackdropFilters(filters);
  gfx::RRectF backdrop_filter_bounds(gfx::RectF(gfx::SizeF(blur->bounds())), 0);
  blur->SetBackdropFilterBounds(backdrop_filter_bounds);

#if defined(OS_WIN) || defined(_MIPS_ARCH_LOONGSON) || defined(ARCH_CPU_ARM64)
#if defined(OS_WIN) || defined(ARCH_CPU_ARM64)
  // Windows has 5.9325% pixels by at most 2: crbug.com/259922
  float percentage_pixels_large_error = 6.0f;
#else
  // Loongson has 8.685% pixels by at most 2: crbug.com/819110
  float percentage_pixels_large_error = 8.7f;
#endif
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 2.f;
  int large_error_allowed = 2;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error,
      percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels,
      large_error_allowed,
      small_error_allowed));
#else
  if (use_skia_vulkan())
    pixel_comparator_ = std::make_unique<FuzzyPixelOffByOneComparator>(true);
#endif

  RunPixelTest(
      background,
      base::FilePath(FILE_PATH_LITERAL("backdrop_filter_blur_outsets.png")));
}

class LayerTreeHostImageFiltersPixelTestLayerList
    : public LayerTreeHostFiltersPixelTest {
 public:
  LayerTreeHostImageFiltersPixelTestLayerList() { SetUseLayerLists(); }

  void SetupTree() override {
    SetInitialRootBounds(gfx::Size(200, 200));
    LayerTreePixelTest::SetupTree();

    Layer* root = layer_tree_host()->root_layer();
    scoped_refptr<SolidColorLayer> background =
        CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorYELLOW);
    CopyProperties(root, background.get());
    root->AddChild(background);

    scoped_refptr<SolidColorLayer> foreground =
        CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorRED);
    CopyProperties(root, foreground.get());
    root->AddChild(foreground);

    EffectNode& effect_node = CreateEffectNode(foreground.get());
    float matrix[20] = {0};
    // This filter does a red-blue swap, so the foreground becomes blue.
    matrix[2] = matrix[6] = matrix[10] = matrix[18] = 1.0f;
    // Set up a crop rect to filter the bottom 200x100 pixels of the foreground.
    SkImageFilter::CropRect crop_rect(SkRect::MakeXYWH(0, 100, 200, 100));
    FilterOperations filters;
    filters.Append(FilterOperation::CreateReferenceFilter(
        sk_make_sp<ColorFilterPaintFilter>(SkColorFilters::Matrix(matrix),
                                           nullptr, &crop_rect)));

    effect_node.filters = filters;
    effect_node.render_surface_reason = RenderSurfaceReason::kFilter;

    // Move the filters origin up by 100 pixels so the crop rect is applied
    // only to the top 100 pixels, not the bottom.
    effect_node.filters_origin = gfx::PointF(0.0f, -100.0f);
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostImageFiltersPixelTestLayerList,
                         ::testing::ValuesIn(kRendererTypes));

TEST_P(LayerTreeHostImageFiltersPixelTestLayerList, NonZeroOrigin) {
  RunPixelTestWithLayerList(
      base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")));
}

class LayerTreeHostBlurFiltersPixelTestGPULayerList
    : public LayerTreeHostFiltersPixelTest {
 public:
  LayerTreeHostBlurFiltersPixelTestGPULayerList() { SetUseLayerLists(); }

  void SetupTree() override {
    SetInitialRootBounds(gfx::Size(200, 200));
    LayerTreePixelTest::SetupTree();

    Layer* root = layer_tree_host()->root_layer();

    scoped_refptr<SolidColorLayer> background =
        CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorTRANSPARENT);
    CopyProperties(root, background.get());
    root->AddChild(background);

    TransformNode& background_transform_node =
        CreateTransformNode(background.get());
    background_transform_node.local.ApplyPerspectiveDepth(200.0);
    background_transform_node.flattens_inherited_transform = true;
    background_transform_node.sorting_context_id = 1;

    // This verifies that the perspective of the clear layer (with black border)
    // does not influence the blending of the green box behind it. Also verifies
    // that the blur is correctly clipped inside the transformed clear layer.
    scoped_refptr<SolidColorLayer> green =
        CreateSolidColorLayer(gfx::Rect(50, 50, 100, 100), kCSSGreen);
    CopyProperties(background.get(), green.get());
    root->AddChild(green);

    std::vector<scoped_refptr<SolidColorLayer>> blur_layers;
    CreateSolidColorLayerPlusBorders(gfx::Rect(0, 0, 120, 120),
                                     SK_ColorTRANSPARENT, 1, SK_ColorBLACK,
                                     blur_layers);
    CopyProperties(background.get(), blur_layers[0].get());
    TransformNode& blur_transform_node =
        CreateTransformNode(blur_layers[0].get(), background_transform_node.id);

    blur_transform_node.local.Translate(55.0, 65.0);
    blur_transform_node.local.RotateAboutXAxis(85.0);
    blur_transform_node.local.RotateAboutYAxis(180.0);
    blur_transform_node.local.RotateAboutZAxis(20.0);
    blur_transform_node.local.Translate(-60.0, -60.0);
    blur_transform_node.flattens_inherited_transform = false;
    blur_transform_node.post_translation = gfx::Vector2dF(30, 30);
    blur_transform_node.sorting_context_id = 1;

    EffectNode& blur_effect_node = CreateEffectNode(blur_layers[0].get());

    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(
        2.f, SkBlurImageFilter::kClamp_TileMode));
    blur_effect_node.backdrop_filters = filters;
    blur_effect_node.render_surface_reason =
        RenderSurfaceReason::kBackdropFilter;
    blur_effect_node.closest_ancestor_with_copy_request_id = 1;

    // TODO(916311): We should be able to set the bounds like this, but the
    // resulting output is clipped incorrectly.
    // gfx::RRectF
    // backdrop_filter_bounds(gfx::RectF(gfx::SizeF(blur->bounds())),0);
    // blur_effect_node.backdrop_filter_bounds.emplace(backdrop_filter_bounds);

    root->AddChild(blur_layers[0]);
    for (unsigned i = 1; i < blur_layers.size(); i++) {
      CopyProperties(blur_layers[0].get(), blur_layers[i].get());
      root->AddChild(blur_layers[i]);
    }
  }
};

// TODO(sgilhuly): Enable these tests for Skia Dawn, and switch over to using
// kRendererTypesGpu.
LayerTreeTest::RendererType const kRendererTypesGpuNonDawn[] = {
    LayerTreeTest::RENDERER_GL,
    LayerTreeTest::RENDERER_SKIA_GL,
#if defined(ENABLE_CC_VULKAN_TESTS)
    LayerTreeTest::RENDERER_SKIA_VK,
#endif  // defined(ENABLE_CC_VULKAN_TESTS)
};

INSTANTIATE_TEST_SUITE_P(PixelResourceTest,
                         LayerTreeHostBlurFiltersPixelTestGPULayerList,
                         ::testing::ValuesIn(kRendererTypesGpuNonDawn));

TEST_P(LayerTreeHostBlurFiltersPixelTestGPULayerList,
       BackdropFilterBlurOffAxis) {
#if defined(OS_WIN) || defined(ARCH_CPU_ARM64)
#if defined(OS_WIN)
  // Windows has 116 pixels off by at most 2: crbug.com/225027
  float percentage_pixels_large_error = 0.3f;  // 116px / (200*200), rounded up
  int large_error_allowed = 2;
#else
  float percentage_pixels_large_error = 0.25f;  // 96px / (200*200), rounded up
  int large_error_allowed = 1;
#endif
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 1.f;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error,
      percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels,
      large_error_allowed,
      small_error_allowed));
#else
  if (use_skia_vulkan())
    pixel_comparator_ = std::make_unique<FuzzyPixelOffByOneComparator>(true);
#endif

  RunPixelTestWithLayerList(
      base::FilePath(FILE_PATH_LITERAL("backdrop_filter_blur_off_axis_.png"))
          .InsertBeforeExtensionASCII(GetRendererSuffix()));
}

TEST_P(LayerTreeHostFiltersPixelTest, BackdropFilterBoundsWithChildren) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);
  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(50, 50, 100, 100), kCSSGreen);
  scoped_refptr<SolidColorLayer> invert =
      CreateSolidColorLayer(gfx::Rect(30, 30, 140, 40), SK_ColorTRANSPARENT);
  scoped_refptr<SolidColorLayer> invert_child1 =
      CreateSolidColorLayer(gfx::Rect(50, -20, 40, 20), kCSSOrange);
  scoped_refptr<SolidColorLayer> invert_child2 =
      CreateSolidColorLayer(gfx::Rect(50, 40, 40, 20), kCSSOrange);
  background->AddChild(green);
  background->AddChild(invert);
  invert->AddChild(invert_child1);
  invert->AddChild(invert_child2);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateInvertFilter(1.f));
  invert->SetBackdropFilters(filters);
  gfx::RRectF backdrop_filter_bounds(gfx::RectF(gfx::SizeF(invert->bounds())),
                                     0);
  invert->SetBackdropFilterBounds(backdrop_filter_bounds);

  RunPixelTest(background, base::FilePath(FILE_PATH_LITERAL(
                               "backdrop_filter_bounds_with_children.png")));
}

class LayerTreeHostFiltersScaledPixelTest
    : public LayerTreeHostFiltersPixelTest {
  void SetupTree() override {
    SetInitialDeviceScaleFactor(device_scale_factor_);
    LayerTreePixelTest::SetupTree();
  }

 protected:
  void RunPixelTestType(int content_size, float device_scale_factor) {
    int half_content = content_size / 2;

    scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
        gfx::Rect(0, 0, content_size, content_size), SK_ColorGREEN);

    // Add a blue layer that completely covers the green layer.
    scoped_refptr<SolidColorLayer> foreground = CreateSolidColorLayer(
        gfx::Rect(0, 0, content_size, content_size), SK_ColorBLUE);
    background->AddChild(foreground);

    // Add an alpha threshold filter to the blue layer which will filter out
    // everything except the lower right corner.
    FilterOperations filters;
    FilterOperation::ShapeRects alpha_shape;
    alpha_shape.emplace_back(half_content, half_content, content_size,
                             content_size);
    filters.Append(
        FilterOperation::CreateAlphaThresholdFilter(alpha_shape, 1.f, 0.f));
    foreground->SetFilters(filters);

    device_scale_factor_ = device_scale_factor;
    RunPixelTest(
        background,
        base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
  }

  float device_scale_factor_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostFiltersScaledPixelTest,
                         ::testing::ValuesIn(kRendererTypes));

TEST_P(LayerTreeHostFiltersScaledPixelTest, StandardDpi) {
  RunPixelTestType(100, 1.f);
}

TEST_P(LayerTreeHostFiltersScaledPixelTest, HiDpi) {
  RunPixelTestType(50, 2.f);
}

TEST_P(LayerTreeHostFiltersPixelTest, NullFilter) {
  scoped_refptr<SolidColorLayer> foreground =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorGREEN);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateReferenceFilter(nullptr));
  foreground->SetFilters(filters);

  RunPixelTest(foreground, base::FilePath(FILE_PATH_LITERAL("green.png")));
}

TEST_P(LayerTreeHostFiltersPixelTest, CroppedFilter) {
  scoped_refptr<SolidColorLayer> foreground =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorGREEN);

  // Check that a filter with a zero-height crop rect crops out its
  // result completely.
  FilterOperations filters;
  SkImageFilter::CropRect cropRect(SkRect::MakeXYWH(0, 0, 100, 0));
  sk_sp<PaintFilter> offset(
      sk_make_sp<OffsetPaintFilter>(0, 0, nullptr, &cropRect));
  filters.Append(FilterOperation::CreateReferenceFilter(offset));
  foreground->SetFilters(filters);

  RunPixelTest(foreground, base::FilePath(FILE_PATH_LITERAL("white.png")));
}

TEST_P(LayerTreeHostFiltersPixelTest, ImageFilterClipped) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorYELLOW);

  scoped_refptr<SolidColorLayer> foreground =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorRED);
  background->AddChild(foreground);

  float matrix[20] = {0};
  // This filter does a red-blue swap, so the foreground becomes blue.
  matrix[2] = matrix[6] = matrix[10] = matrix[18] = 1.0f;
  // We filter only the bottom 200x100 pixels of the foreground.
  SkImageFilter::CropRect crop_rect(SkRect::MakeXYWH(0, 100, 200, 100));
  FilterOperations filters;
  filters.Append(
      FilterOperation::CreateReferenceFilter(sk_make_sp<ColorFilterPaintFilter>(
          SkColorFilters::Matrix(matrix), nullptr, &crop_rect)));

  // Make the foreground layer's render surface be clipped by the background
  // layer.
  background->SetMasksToBounds(true);
  foreground->SetFilters(filters);

  // Then we translate the foreground up by 100 pixels in Y, so the cropped
  // region is moved to to the top. This ensures that the crop rect is being
  // correctly transformed in skia by the amount of clipping that the
  // compositor performs.
  gfx::Transform transform;
  transform.Translate(0.0, -100.0);
  foreground->SetTransform(transform);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")));
}

TEST_P(LayerTreeHostFiltersPixelTest, ImageFilterScaled) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  gfx::Rect rect(50, 50, 100, 100);

  const int kInset = 3;
  for (int i = 0; !rect.IsEmpty(); ++i) {
    scoped_refptr<SolidColorLayer> layer =
        CreateSolidColorLayer(rect, (i & 1) ? SK_ColorWHITE : SK_ColorRED);

    gfx::Transform transform;
    transform.Translate(rect.width() / 2.0, rect.height() / 2.0);
    transform.RotateAboutZAxis(30.0);
    transform.Translate(-rect.width() / 2.0, -rect.height() / 2.0);
    layer->SetTransform(transform);

    background->AddChild(layer);

    rect.Inset(kInset, kInset);
  }

  scoped_refptr<SolidColorLayer> filter =
      CreateSolidColorLayer(gfx::Rect(100, 0, 100, 200), SK_ColorTRANSPARENT);

  background->AddChild(filter);

  // Apply a scale to |background| so that we can see any scaling artifacts
  // that may appear.
  gfx::Transform background_transform;
  static float scale = 1.1f;
  background_transform.Scale(scale, scale);
  background->SetTransform(background_transform);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateGrayscaleFilter(1.0f));
  filter->SetBackdropFilters(filters);
  filter->ClearBackdropFilterBounds();

#if defined(OS_WIN) || defined(_MIPS_ARCH_LOONGSON) || defined(ARCH_CPU_ARM64)
#if defined(OS_WIN)
  // Windows has 153 pixels off by at most 2: crbug.com/225027
  float percentage_pixels_large_error = 0.3825f;  // 153px / (200*200)
  int large_error_allowed = 2;
  // Windows using Dawn D3D12 has 166 pixels off by 1.
  if (use_d3d12()) {
    percentage_pixels_large_error = 0.415f;  // 166px / (200*200)
    large_error_allowed = 1;
  }
#elif defined(_MIPS_ARCH_LOONGSON)
  // Loongson has 2 pixels off by at most 2: crbug.com/819075
  float percentage_pixels_large_error = 0.005f;  // 2px / (200*200)
  int large_error_allowed = 2;
#else
  float percentage_pixels_large_error = 0.0325f;  // 13px / (200*200)
  int large_error_allowed = 1;
#endif
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 1.f;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error, percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels, large_error_allowed,
      small_error_allowed));
#endif

  RunPixelTest(
      background,
      base::FilePath(FILE_PATH_LITERAL("backdrop_filter_on_scaled_layer_.png"))
          .InsertBeforeExtensionASCII(GetRendererSuffix()));
}

TEST_P(LayerTreeHostFiltersPixelTest, BackdropFilterRotated) {
  // Add a white background with a rotated red rect in the center.
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  gfx::Rect rect(50, 50, 100, 100);

  scoped_refptr<SolidColorLayer> layer =
      CreateSolidColorLayer(rect, SK_ColorRED);

  gfx::Transform transform;
  transform.Translate(rect.width() / 2.0, rect.height() / 2.0);
  transform.RotateAboutZAxis(30.0);
  transform.Translate(-rect.width() / 2.0, -rect.height() / 2.0);
  layer->SetTransform(transform);

  background->AddChild(layer);

  // Add a slightly transparent blue layer.
  SkColor transparent_blue = SkColorSetARGB(0x23, 0x00, 0x00, 0xFF);
  scoped_refptr<SolidColorLayer> filter_layer =
      CreateSolidColorLayer(gfx::Rect(100, 0, 100, 200), transparent_blue);

  // Add some rotation so that we can see that it blurs only under the layer.
  gfx::Transform transform_filter;
  transform_filter.RotateAboutZAxis(10.0);
  filter_layer->SetTransform(transform_filter);

  // Add a blur filter to the blue layer.
  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(
      5.0f, SkBlurImageFilter::kClamp_TileMode));
  filter_layer->SetBackdropFilters(filters);
  gfx::RRectF backdrop_filter_bounds(
      gfx::RectF(gfx::SizeF(filter_layer->bounds())), 0);
  filter_layer->SetBackdropFilterBounds(backdrop_filter_bounds);
  background->AddChild(filter_layer);

  // Allow some fuzziness so that this doesn't fail when Skia makes minor
  // changes to blur or rectangle rendering.
  float percentage_pixels_large_error = 4.f;
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 2.f;
  int large_error_allowed = 2;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error, percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels, large_error_allowed,
      small_error_allowed));

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL("backdrop_filter_rotated_.png"))
                   .InsertBeforeExtensionASCII(GetRendererSuffix()));
}

TEST_P(LayerTreeHostFiltersPixelTest, ImageRenderSurfaceScaled) {
  // A filter will cause a render surface to be used.  Here we force the
  // render surface on, and scale the result to make sure that we rasterize at
  // the correct resolution.
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(300, 300), SK_ColorBLUE);

  scoped_refptr<SolidColorLayer> render_surface_layer =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  gfx::Rect rect(50, 50, 100, 100);

  scoped_refptr<SolidColorLayer> child =
      CreateSolidColorLayer(rect, SK_ColorRED);

  gfx::Transform transform;
  transform.Translate(rect.width() / 2.0, rect.height() / 2.0);
  transform.RotateAboutZAxis(30.0);
  transform.Translate(-rect.width() / 2.0, -rect.height() / 2.0);
  child->SetTransform(transform);

  render_surface_layer->AddChild(child);

  gfx::Transform render_surface_transform;
  render_surface_transform.Scale(1.5f, 1.5f);
  render_surface_layer->SetTransform(render_surface_transform);
  render_surface_layer->SetForceRenderSurfaceForTesting(true);

  background->AddChild(render_surface_layer);

  float percentage_pixels_large_error = 0.0f;
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 0.0f;
  int large_error_allowed = 0;
  int small_error_allowed = 0;
  if (use_software_renderer()) {
    // Software has some huge differences in the AA'd pixels on the different
    // trybots. See crbug.com/452198.
    percentage_pixels_large_error = 0.686f;
    average_error_allowed_in_bad_pixels = 16.f;
    large_error_allowed = 17;
  } else if (use_d3d12()) {
    // Windows using Dawn D3D12 has 25 pixels off by 1.
    percentage_pixels_large_error = 0.028;
    average_error_allowed_in_bad_pixels = 1.f;
    large_error_allowed = 1;
  }
  pixel_comparator_.reset(new FuzzyPixelComparator(
      /*discard_alpha=*/true, percentage_pixels_large_error,
      percentage_pixels_small_error, average_error_allowed_in_bad_pixels,
      large_error_allowed, small_error_allowed));

  RunPixelTest(
      background,
      base::FilePath(FILE_PATH_LITERAL("scaled_render_surface_layer_.png"))
          .InsertBeforeExtensionASCII(GetRendererSuffix()));
}

// TODO(sgilhuly): Enable these tests for Skia Dawn, and switch over to using
// kRendererTypes.
using LayerTreeHostFiltersPixelTestNonDawn = LayerTreeHostFiltersPixelTest;

LayerTreeTest::RendererType const kRendererTypesNonDawn[] = {
    LayerTreeTest::RENDERER_GL,
    LayerTreeTest::RENDERER_SKIA_GL,
    LayerTreeTest::RENDERER_SOFTWARE,
#if defined(ENABLE_CC_VULKAN_TESTS)
    LayerTreeTest::RENDERER_SKIA_VK,
#endif  // defined(ENABLE_CC_VULKAN_TESTS)
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostFiltersPixelTestNonDawn,
                         ::testing::ValuesIn(kRendererTypesNonDawn));

TEST_P(LayerTreeHostFiltersPixelTestNonDawn, ZoomFilter) {
  scoped_refptr<SolidColorLayer> root =
      CreateSolidColorLayer(gfx::Rect(300, 300), SK_ColorWHITE);

  // Create the pattern. Two blue/yellow side by side blocks with a horizontal
  // green strip.
  scoped_refptr<SolidColorLayer> left =
      CreateSolidColorLayer(gfx::Rect(0, 0, 100, 150), SK_ColorBLUE);
  root->AddChild(left);
  scoped_refptr<SolidColorLayer> right =
      CreateSolidColorLayer(gfx::Rect(100, 0, 200, 150), SK_ColorYELLOW);
  root->AddChild(right);
  scoped_refptr<SolidColorLayer> horizontal_strip =
      CreateSolidColorLayer(gfx::Rect(0, 10, 300, 10), SK_ColorGREEN);
  root->AddChild(horizontal_strip);

  // Test a zoom that extends past the edge of the screen.
  scoped_refptr<SolidColorLayer> border_edge_zoom =
      CreateSolidColorLayer(gfx::Rect(-10, -10, 50, 50), SK_ColorTRANSPARENT);
  FilterOperations border_filters;
  border_filters.Append(
      FilterOperation::CreateZoomFilter(2.f /* zoom */, 0 /* inset */));
  border_edge_zoom->SetBackdropFilters(border_filters);
  gfx::RectF border_filter_bounds(gfx::SizeF(border_edge_zoom->bounds()));
  border_edge_zoom->SetBackdropFilterBounds(
      gfx::RRectF(border_filter_bounds, 0));
  root->AddChild(border_edge_zoom);

  // Test a zoom that extends past the edge of the screen.
  scoped_refptr<SolidColorLayer> top_edge_zoom =
      CreateSolidColorLayer(gfx::Rect(70, -10, 50, 50), SK_ColorTRANSPARENT);
  FilterOperations top_filters;
  top_filters.Append(
      FilterOperation::CreateZoomFilter(2.f /* zoom */, 0 /* inset */));
  top_edge_zoom->SetBackdropFilters(top_filters);
  gfx::RectF top_filter_bounds(gfx::SizeF(top_edge_zoom->bounds()));
  top_edge_zoom->SetBackdropFilterBounds(gfx::RRectF(top_filter_bounds, 0));
  root->AddChild(top_edge_zoom);

  // Test a zoom that is fully within the screen.
  scoped_refptr<SolidColorLayer> contained_zoom =
      CreateSolidColorLayer(gfx::Rect(150, 5, 50, 50), SK_ColorTRANSPARENT);
  FilterOperations mid_filters;
  mid_filters.Append(
      FilterOperation::CreateZoomFilter(2.f /* zoom */, 0 /* inset */));
  contained_zoom->SetBackdropFilters(mid_filters);
  gfx::RectF mid_filter_bounds(gfx::SizeF(contained_zoom->bounds()));
  contained_zoom->SetBackdropFilterBounds(gfx::RRectF(mid_filter_bounds, 0));
  root->AddChild(contained_zoom);

#if defined(OS_WIN)
  // Windows has 1 pixel off by 1: crbug.com/259915
  float percentage_pixels_large_error = 0.00111112f;  // 1px / (300*300)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 1.f;
  int large_error_allowed = 1;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error, percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels, large_error_allowed,
      small_error_allowed));
#endif

  RunPixelTest(std::move(root),
               base::FilePath(FILE_PATH_LITERAL("zoom_filter_.png"))
                   .InsertBeforeExtensionASCII(GetRendererSuffix()));
}

TEST_P(LayerTreeHostFiltersPixelTest, RotatedFilter) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(300, 300), SK_ColorWHITE);

  gfx::Rect rect(50, 50, 100, 100);

  scoped_refptr<SolidColorLayer> child =
      CreateSolidColorLayer(rect, SK_ColorBLUE);

  gfx::Transform transform;
  transform.Translate(50.0f, 50.0f);
  transform.RotateAboutZAxis(1.0);
  transform.Translate(-50.0f, -50.0f);
  child->SetTransform(transform);
  FilterOperations filters;
  // We need a clamping brightness filter here to force the Skia filter path.
  filters.Append(FilterOperation::CreateBrightnessFilter(1.1f));
  filters.Append(FilterOperation::CreateGrayscaleFilter(1.0f));
  child->SetFilters(filters);

  background->AddChild(child);

#if defined(OS_WIN) || defined(OS_FUCHSIA)
#if defined (ARCH_CPU_ARM64)
  // Windows ARM64 and Fuchsia has some pixels difference
  // crbug.com/1029728, crbug.com/1048249
  float percentage_pixels_large_error = 0.391112f;
  float average_error_allowed_in_bad_pixels = 1.1f;
  int large_error_allowed = 3;
#elif defined(OS_WIN)
  // Windows has 1 pixel off by 1: crbug.com/259915
  float percentage_pixels_large_error = 0.00111112f;  // 1px / (300*300)
  float average_error_allowed_in_bad_pixels = 1.f;
  int large_error_allowed = 1;
  // Windows using Dawn D3D12 has 104 pixels off by 1.
  if (use_d3d12())
    percentage_pixels_large_error = 0.115556f;  // 104px / (300*300)
#else
  float percentage_pixels_large_error = 0.0f;  // 1px / (300*300)
  float average_error_allowed_in_bad_pixels = 0.0f;
  int large_error_allowed = 0;
#endif
  float percentage_pixels_small_error = 0.0f;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error, percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels, large_error_allowed,
      small_error_allowed));
#endif

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL("rotated_filter_.png"))
                   .InsertBeforeExtensionASCII(GetRendererSuffix()));
}

TEST_P(LayerTreeHostFiltersPixelTest, RotatedDropShadowFilter) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(300, 300), SK_ColorWHITE);

  gfx::Rect rect(50, 50, 100, 100);

  // Use a border to defeat solid color detection to force a tile quad.
  // This is intended to test render pass removal optimizations.
  SolidColorContentLayerClient blue_client(SK_ColorBLUE, rect.size(), 1,
                                           SK_ColorBLACK);
  scoped_refptr<PictureLayer> child = PictureLayer::Create(&blue_client);
  child->SetBounds(rect.size());
  child->SetPosition(gfx::PointF(rect.origin()));
  child->SetIsDrawable(true);

  gfx::Transform transform;
  transform.Translate(50.0f, 50.0f);
  transform.RotateAboutZAxis(1.0);
  transform.Translate(-50.0f, -50.0f);
  child->SetTransform(transform);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateDropShadowFilter(
      gfx::Point(10.0f, 10.0f), 0.0f, SK_ColorBLACK));
  child->SetFilters(filters);

  background->AddChild(child);

#if defined(OS_WIN) || defined(ARCH_CPU_ARM64)
#if (defined(OS_WIN) && defined(ARCH_CPU_ARM64)) || defined(OS_FUCHSIA)
  // Windows ARM64 has some pixels difference: crbug.com/1029729
  float percentage_pixels_large_error = 0.89f;
  float average_error_allowed_in_bad_pixels = 5.f;
  int large_error_allowed = 17;
#else
  // Windows and ARM64 have 3 pixels off by 1: crbug.com/259915
  float percentage_pixels_large_error = 0.00333334f;  // 3px / (300*300)
  float average_error_allowed_in_bad_pixels = 1.f;
  int large_error_allowed = 1;
  // Windows using Dawn D3D12 has 22 pixels off by 1.
  if (use_d3d12())
    percentage_pixels_large_error = 0.02445;  // 22px / (300*300)
#endif
  float percentage_pixels_small_error = 0.0f;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error, percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels, large_error_allowed,
      small_error_allowed));
#else
  if (use_skia_vulkan())
    pixel_comparator_ = std::make_unique<FuzzyPixelOffByOneComparator>(true);
#endif

  RunPixelTest(
      background,
      base::FilePath(FILE_PATH_LITERAL("rotated_drop_shadow_filter_.png"))
          .InsertBeforeExtensionASCII(GetRendererSuffix()));
}

TEST_P(LayerTreeHostFiltersPixelTest, TranslatedFilter) {
  scoped_refptr<Layer> clip = Layer::Create();
  clip->SetBounds(gfx::Size(300, 300));
  clip->SetMasksToBounds(true);

  scoped_refptr<Layer> parent = Layer::Create();
  parent->SetPosition(gfx::PointF(30.f, 30.f));

  gfx::Rect child_rect(100, 100);
  // Use two colors to bypass solid color detection, so we get a tile quad.
  // This is intended to test render pass removal optimizations.
  FakeContentLayerClient client;
  client.set_bounds(child_rect.size());
  PaintFlags flags;
  flags.setColor(SK_ColorGREEN);
  client.add_draw_rect(child_rect, flags);
  flags.setColor(SK_ColorBLUE);
  client.add_draw_rect(gfx::Rect(100, 50), flags);
  scoped_refptr<PictureLayer> child = PictureLayer::Create(&client);
  child->SetBounds(child_rect.size());
  child->SetIsDrawable(true);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateOpacityFilter(0.5f));
  child->SetFilters(filters);

  // This layer will be clipped by |clip|, so the RenderPass created for
  // |child| will only have one visible quad.
  scoped_refptr<SolidColorLayer> grand_child =
      CreateSolidColorLayer(gfx::Rect(-300, -300, 100, 100), SK_ColorRED);

  child->AddChild(grand_child);
  parent->AddChild(child);
  clip->AddChild(parent);

  if (use_software_renderer() || renderer_type_ == RENDERER_SKIA_DAWN)
    pixel_comparator_ = std::make_unique<FuzzyPixelOffByOneComparator>(true);

  RunPixelTest(clip, base::FilePath(
                         FILE_PATH_LITERAL("translated_blue_green_alpha.png")));
}

TEST_P(LayerTreeHostFiltersPixelTest, EnlargedTextureWithAlphaThresholdFilter) {
  // Rectangles chosen so that if flipped, the test will fail.
  gfx::Rect rect1(10, 10, 10, 15);
  gfx::Rect rect2(20, 25, 70, 65);

  scoped_refptr<SolidColorLayer> child1 =
      CreateSolidColorLayer(rect1, SK_ColorRED);
  scoped_refptr<SolidColorLayer> child2 =
      CreateSolidColorLayer(rect2, SK_ColorGREEN);
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorBLUE);
  scoped_refptr<SolidColorLayer> filter_layer =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  // Make sure a transformation does not cause misregistration of the filter
  // and source texture.
  gfx::Transform filter_transform;
  filter_transform.Scale(2.f, 2.f);
  filter_layer->SetTransform(filter_transform);
  filter_layer->AddChild(child1);
  filter_layer->AddChild(child2);

  rect1.Inset(-5, -5);
  rect2.Inset(-5, -5);
  FilterOperation::ShapeRects alpha_shape = {rect1, rect2};
  FilterOperations filters;
  filters.Append(
      FilterOperation::CreateAlphaThresholdFilter(alpha_shape, 0.f, 0.f));
  filter_layer->SetFilters(filters);

  background->AddChild(filter_layer);

  // Force the allocation a larger textures.
  set_enlarge_texture_amount(gfx::Size(50, 50));

  RunPixelTest(
      background,
      base::FilePath(FILE_PATH_LITERAL("enlarged_texture_on_threshold.png")));
}

TEST_P(LayerTreeHostFiltersPixelTest, EnlargedTextureWithCropOffsetFilter) {
  // Rectangles chosen so that if flipped, the test will fail.
  gfx::Rect rect1(10, 10, 10, 15);
  gfx::Rect rect2(20, 25, 70, 65);

  scoped_refptr<SolidColorLayer> child1 =
      CreateSolidColorLayer(rect1, SK_ColorRED);
  scoped_refptr<SolidColorLayer> child2 =
      CreateSolidColorLayer(rect2, SK_ColorGREEN);
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorBLUE);
  scoped_refptr<SolidColorLayer> filter_layer =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  // Make sure a transformation does not cause misregistration of the filter
  // and source texture.
  gfx::Transform filter_transform;
  filter_transform.Scale(2.f, 2.f);
  filter_layer->SetTransform(filter_transform);
  filter_layer->AddChild(child1);
  filter_layer->AddChild(child2);

  FilterOperations filters;
  SkImageFilter::CropRect cropRect(SkRect::MakeXYWH(10, 10, 80, 80));
  filters.Append(FilterOperation::CreateReferenceFilter(
      sk_make_sp<OffsetPaintFilter>(0, 0, nullptr, &cropRect)));
  filter_layer->SetFilters(filters);

  background->AddChild(filter_layer);

  // Force the allocation a larger textures.
  set_enlarge_texture_amount(gfx::Size(50, 50));

  RunPixelTest(
      background,
      base::FilePath(FILE_PATH_LITERAL("enlarged_texture_on_crop_offset.png")));
}

TEST_P(LayerTreeHostFiltersPixelTest, BlurFilterWithClip) {
  scoped_refptr<SolidColorLayer> child1 =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorBLUE);
  scoped_refptr<SolidColorLayer> child2 =
      CreateSolidColorLayer(gfx::Rect(20, 20, 160, 160), SK_ColorWHITE);
  scoped_refptr<SolidColorLayer> child3 =
      CreateSolidColorLayer(gfx::Rect(40, 40, 20, 30), SK_ColorRED);
  scoped_refptr<SolidColorLayer> child4 =
      CreateSolidColorLayer(gfx::Rect(60, 70, 100, 90), SK_ColorGREEN);
  scoped_refptr<SolidColorLayer> filter_layer =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  filter_layer->AddChild(child1);
  filter_layer->AddChild(child2);
  filter_layer->AddChild(child3);
  filter_layer->AddChild(child4);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(
      2.f, SkBlurImageFilter::kClamp_TileMode));
  filter_layer->SetFilters(filters);

  // Force the allocation a larger textures.
  set_enlarge_texture_amount(gfx::Size(50, 50));

#if defined(OS_WIN) || defined(ARCH_CPU_ARM64)
#if defined(OS_WIN)
  // Windows has 1880 pixels off by 1: crbug.com/259915
  float percentage_pixels_large_error = 4.7f;  // 1880px / (200*200)
#else
  // Differences in floating point calculation on ARM means a small percentage
  // of pixels will have small differences.
  float percentage_pixels_large_error = 2.76f;  // 1104px / (200*200)
#endif
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 1.f;
  int large_error_allowed = 2;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error, percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels, large_error_allowed,
      small_error_allowed));
#endif

  RunPixelTest(filter_layer,
               base::FilePath(FILE_PATH_LITERAL("blur_filter_with_clip_.png"))
                   .InsertBeforeExtensionASCII(GetRendererSuffix()));
}

TEST_P(LayerTreeHostFiltersPixelTestGPU, FilterWithGiantCropRect) {
  scoped_refptr<SolidColorLayer> tree = BuildFilterWithGiantCropRect(true);
  RunPixelTest(tree, base::FilePath(
                         FILE_PATH_LITERAL("filter_with_giant_crop_rect.png")));
}

TEST_P(LayerTreeHostFiltersPixelTestGPU, FilterWithGiantCropRectNoClip) {
  scoped_refptr<SolidColorLayer> tree = BuildFilterWithGiantCropRect(false);
  RunPixelTest(tree, base::FilePath(
                         FILE_PATH_LITERAL("filter_with_giant_crop_rect.png")));
}

class BackdropFilterOffsetTest : public LayerTreeHostFiltersPixelTest {
 protected:
  void RunPixelTestType(int device_scale_factor) {
    scoped_refptr<Layer> root =
        CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);
    scoped_refptr<SolidColorLayer> background =
        CreateSolidColorLayer(gfx::Rect(100, 120), SK_ColorBLACK);
    root->AddChild(background);
    scoped_refptr<SolidColorLayer> filtered = CreateSolidColorLayer(
        gfx::Rect(0, 100, 200, 100), SkColorSetA(SK_ColorGREEN, 127));
    FilterOperations filters;
    // TODO(989329): This test actually also tests how the OffsetPaintFilter
    // handles the edge condition. Because the OffsetPaintFilter is pulling
    // content from outside the filter region (just the bottom 200x100 square),
    // it must create content for all but the bottom 20px of the filter rect.
    // The GPU implementation of OffsetPaintFilter effectively clamps to the
    // edge pixels and copies them into the top 80px of the image. The CPU
    // implementation, on the other hand, pulls in transparent pixels for those
    // 80px. The behavior is basically unspecified, though for blur filters the
    // explicitly specified behavior is edgemode:duplicate, as seen in [1]. And
    // the default for svg filters, including feOffset, is also duplicate.
    // [1] https://drafts.fxtf.org/filter-effects-2/#backdrop-filter-operation
    // [2] https://www.w3.org/TR/SVG11/filters.html#feConvolveMatrixElementEdgeModeAttribute
    filters.Append(FilterOperation::CreateReferenceFilter(
        sk_make_sp<OffsetPaintFilter>(0, 80, nullptr)));
    filtered->SetBackdropFilters(filters);
    filtered->ClearBackdropFilterBounds();
    root->AddChild(filtered);
    // This should appear as a grid of 4 100x100 squares which are:
    // BLACK       WHITE
    // DARK GREEN*  LIGHT GREEN
    //
    // *except for software (see crbug.com/989329) which will be a
    // dark-light-dark horizontal sandwich.
    device_scale_factor_ = device_scale_factor;

    base::FilePath expected_result =
        base::FilePath(FILE_PATH_LITERAL("offset_backdrop_filter_.png"));
    expected_result = expected_result.InsertBeforeExtensionASCII(
        base::NumberToString(device_scale_factor) + "x");
    if (use_software_renderer()) {
      expected_result = expected_result.InsertBeforeExtensionASCII("_sw");
    }
    RunPixelTest(std::move(root), expected_result);
  }

 private:
  // LayerTreePixelTest overrides
  void SetupTree() override {
    SetInitialDeviceScaleFactor(device_scale_factor_);
    LayerTreeHostFiltersPixelTest::SetupTree();
  }

  float device_scale_factor_ = 1;
};

INSTANTIATE_TEST_SUITE_P(All,
                         BackdropFilterOffsetTest,
                         ::testing::ValuesIn(kRendererTypes));

TEST_P(BackdropFilterOffsetTest, StandardDpi) {
  RunPixelTestType(1.f);
}

TEST_P(BackdropFilterOffsetTest, HiDpi) {
  RunPixelTestType(2.f);
}

class BackdropFilterInvertTest : public LayerTreeHostFiltersPixelTest {
 protected:
  void RunPixelTestType(int device_scale_factor) {
    scoped_refptr<Layer> root =
        CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
    scoped_refptr<SolidColorLayer> filtered =
        CreateSolidColorLayer(gfx::Rect(50, 50, 100, 100), SK_ColorTRANSPARENT);
    FilterOperations filters;
    filters.Append(FilterOperation::CreateInvertFilter(1.0));
    filtered->SetBackdropFilters(filters);
    gfx::RRectF backdrop_filter_bounds(
        gfx::RectF(gfx::SizeF(filtered->bounds())), 20);
    filtered->SetBackdropFilterBounds(backdrop_filter_bounds);
    root->AddChild(filtered);
    device_scale_factor_ = device_scale_factor;
    base::FilePath expected_result =
        base::FilePath(FILE_PATH_LITERAL("invert_backdrop_filter_.png"));
    expected_result = expected_result.InsertBeforeExtensionASCII(
        base::NumberToString(device_scale_factor) + "x");
    if (use_software_renderer()) {
      expected_result = expected_result.InsertBeforeExtensionASCII("_sw");
    } else if (use_d3d12()) {
      // Windows using Dawn D3D12 has 16 pixels off by 1.
      float percentage_pixels_large_error = 0.04f;  // 16px / (200*200)
      float average_error_allowed_in_bad_pixels = 1.f;
      int large_error_allowed = 1;
      float percentage_pixels_small_error = 0.0f;
      int small_error_allowed = 0;
      pixel_comparator_.reset(new FuzzyPixelComparator(
          true,  // discard_alpha
          percentage_pixels_large_error, percentage_pixels_small_error,
          average_error_allowed_in_bad_pixels, large_error_allowed,
          small_error_allowed));
    }
    RunPixelTest(std::move(root), expected_result);
  }

 private:
  // LayerTreePixelTest overrides
  void SetupTree() override {
    SetInitialDeviceScaleFactor(device_scale_factor_);
    LayerTreeHostFiltersPixelTest::SetupTree();
  }

  float device_scale_factor_ = 1;
};

INSTANTIATE_TEST_SUITE_P(All,
                         BackdropFilterInvertTest,
                         ::testing::ValuesIn(kRendererTypes));

TEST_P(BackdropFilterInvertTest, StandardDpi) {
  RunPixelTestType(1.f);
}

TEST_P(BackdropFilterInvertTest, HiDpi) {
  RunPixelTestType(2.f);
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
