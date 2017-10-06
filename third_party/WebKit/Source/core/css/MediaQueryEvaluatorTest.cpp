// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaQueryEvaluator.h"

#include <memory>
#include "core/css/MediaList.h"
#include "core/css/MediaValuesCached.h"
#include "core/css/MediaValuesInitialViewport.h"
#include "core/frame/LocalFrameView.h"
#include "core/media_type_names.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/wtf/text/StringBuilder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

typedef struct {
  const char* input;
  const bool output;
} MediaQueryEvaluatorTestCase;

MediaQueryEvaluatorTestCase g_screen_test_cases[] = {
    {"", 1},
    {" ", 1},
    {"screen", 1},
    {"screen and (color)", 1},
    {"not screen and (color)", 0},
    {"screen and (device-aspect-ratio: 16/9)", 0},
    {"screen and (device-aspect-ratio: 1/1)", 1},
    {"all and (min-color: 2)", 1},
    {"all and (min-color: 32)", 0},
    {"all and (min-color-index: 0)", 1},
    {"all and (min-color-index: 1)", 0},
    {"all and (monochrome)", 0},
    {"all and (min-monochrome: 0)", 1},
    {"all and (grid: 0)", 1},
    {"(resolution: 2dppx)", 1},
    {"(resolution: 1dppx)", 0},
    {"(orientation: portrait)", 1},
    {"(orientation: landscape)", 0},
    {"(orientation: url(portrait))", 0},
    {"(orientation: #portrait)", 0},
    {"(orientation: @portrait)", 0},
    {"(orientation: 'portrait')", 0},
    {"(orientation: @junk portrait)", 0},
    {"screen and (orientation: @portrait) and (max-width: 1000px)", 0},
    {"screen and (orientation: @portrait), (max-width: 1000px)", 1},
    {"tv and (scan: progressive)", 0},
    {"(pointer: coarse)", 0},
    {"(pointer: fine)", 1},
    {"(hover: hover)", 1},
    {"(hover: on-demand)", 0},
    {"(hover: none)", 0},
    {"(display-mode)", 1},
    {"(display-mode: fullscreen)", 0},
    {"(display-mode: standalone)", 0},
    {"(display-mode: minimal-ui)", 0},
    {"(display-mode: browser)", 1},
    {"(display-mode: min-browser)", 0},
    {"(display-mode: url(browser))", 0},
    {"(display-mode: #browser)", 0},
    {"(display-mode: @browser)", 0},
    {"(display-mode: 'browser')", 0},
    {"(display-mode: @junk browser)", 0},
    {"(shape: rect)", 1},
    {"(shape: round)", 0},
    {"(max-device-aspect-ratio: 4294967295/1)", 1},
    {"(min-device-aspect-ratio: 1/4294967296)", 1},
    {0, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_monochrome_test_cases[] = {
    {"(color)", 0},
    {"(monochrome)", 1},
    {0, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_viewport_test_cases[] = {
    {"all and (min-width: 500px)", 1},
    {"(min-width: 500px)", 1},
    {"(min-width: 501px)", 0},
    {"(max-width: 500px)", 1},
    {"(max-width: 499px)", 0},
    {"(width: 500px)", 1},
    {"(width: 501px)", 0},
    {"(min-height: 500px)", 1},
    {"(min-height: 501px)", 0},
    {"(min-height: 500.001px)", 0},
    {"(max-height: 500px)", 1},
    {"(max-height: 499.999px)", 0},
    {"(max-height: 499px)", 0},
    {"(height: 500px)", 1},
    {"(height: 500.001px)", 0},
    {"(height: 499.999px)", 0},
    {"(height: 501px)", 0},
    {"(height)", 1},
    {"(width)", 1},
    {"(width: whatisthis)", 0},
    {"screen and (min-width: 400px) and (max-width: 700px)", 1},
    {"(max-aspect-ratio: 4294967296/1)", 1},
    {"(min-aspect-ratio: 1/4294967295)", 1},
    {0, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_float_viewport_test_cases[] = {
    {"all and (min-width: 600.5px)", 1},
    {"(min-width: 600px)", 1},
    {"(min-width: 600.5px)", 1},
    {"(min-width: 601px)", 0},
    {"(max-width: 600px)", 0},
    {"(max-width: 600.5px)", 1},
    {"(max-width: 601px)", 1},
    {"(width: 600.5px)", 1},
    {"(width: 601px)", 0},
    {"(min-height: 700px)", 1},
    {"(min-height: 700.125px)", 1},
    {"(min-height: 701px)", 0},
    {"(min-height: 700.126px)", 0},
    {"(max-height: 701px)", 1},
    {"(max-height: 700.125px)", 1},
    {"(max-height: 700px)", 0},
    {"(height: 700.125px)", 1},
    {"(height: 700.126px)", 0},
    {"(height: 700.124px)", 0},
    {"(height: 701px)", 0},
    {0, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_float_non_friendly_viewport_test_cases[] = {
    {"(min-width: 821px)", 1},
    {"(max-width: 821px)", 1},
    {"(width: 821px)", 1},
    {"(min-height: 821px)", 1},
    {"(max-height: 821px)", 1},
    {"(height: 821px)", 1},
    {"(width: 100vw)", 1},
    {"(height: 100vh)", 1},
    {0, 0}  // Do not remove the terminator line.
};

MediaQueryEvaluatorTestCase g_print_test_cases[] = {
    {"print and (min-resolution: 1dppx)", 1},
    {"print and (min-resolution: 118dpcm)", 1},
    {"print and (min-resolution: 119dpcm)", 0},
    {0, 0}  // Do not remove the terminator line.
};

void TestMQEvaluator(MediaQueryEvaluatorTestCase* test_cases,
                     const MediaQueryEvaluator& media_query_evaluator) {
  RefPtr<MediaQuerySet> query_set = nullptr;
  for (unsigned i = 0; test_cases[i].input; ++i) {
    query_set = MediaQuerySet::Create(test_cases[i].input);
    EXPECT_EQ(test_cases[i].output, media_query_evaluator.Eval(*query_set))
        << "Query: " << test_cases[i].input;
  }
}

TEST(MediaQueryEvaluatorTest, Cached) {
  MediaValuesCached::MediaValuesCachedData data;
  data.viewport_width = 500;
  data.viewport_height = 500;
  data.device_width = 500;
  data.device_height = 500;
  data.device_pixel_ratio = 2.0;
  data.color_bits_per_component = 24;
  data.monochrome_bits_per_component = 0;
  data.primary_pointer_type = kPointerTypeFine;
  data.primary_hover_type = kHoverTypeHover;
  data.default_font_size = 16;
  data.three_d_enabled = true;
  data.media_type = MediaTypeNames::screen;
  data.strict_mode = true;
  data.display_mode = kWebDisplayModeBrowser;
  data.display_shape = kDisplayShapeRect;

  // Default values.
  {
    MediaValues* media_values = MediaValuesCached::Create(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_screen_test_cases, media_query_evaluator);
    TestMQEvaluator(g_viewport_test_cases, media_query_evaluator);
  }

  // Print values.
  {
    data.media_type = MediaTypeNames::print;
    MediaValues* media_values = MediaValuesCached::Create(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_print_test_cases, media_query_evaluator);
    data.media_type = MediaTypeNames::screen;
  }

  // Monochrome values.
  {
    data.color_bits_per_component = 0;
    data.monochrome_bits_per_component = 8;
    MediaValues* media_values = MediaValuesCached::Create(data);
    MediaQueryEvaluator media_query_evaluator(*media_values);
    TestMQEvaluator(g_monochrome_test_cases, media_query_evaluator);
    data.color_bits_per_component = 24;
    data.monochrome_bits_per_component = 0;
  }
}

TEST(MediaQueryEvaluatorTest, Dynamic) {
  std::unique_ptr<DummyPageHolder> page_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  page_holder->GetFrameView().SetMediaType(MediaTypeNames::screen);

  MediaQueryEvaluator media_query_evaluator(&page_holder->GetFrame());
  TestMQEvaluator(g_viewport_test_cases, media_query_evaluator);
  page_holder->GetFrameView().SetMediaType(MediaTypeNames::print);
  TestMQEvaluator(g_print_test_cases, media_query_evaluator);
}

TEST(MediaQueryEvaluatorTest, DynamicNoView) {
  std::unique_ptr<DummyPageHolder> page_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  LocalFrame* frame = &page_holder->GetFrame();
  page_holder.reset();
  ASSERT_EQ(nullptr, frame->View());
  MediaQueryEvaluator media_query_evaluator(frame);
  RefPtr<MediaQuerySet> query_set = MediaQuerySet::Create("foobar");
  EXPECT_FALSE(media_query_evaluator.Eval(*query_set));
}

TEST(MediaQueryEvaluatorTest, CachedFloatViewport) {
  MediaValuesCached::MediaValuesCachedData data;
  data.viewport_width = 600.5;
  data.viewport_height = 700.125;
  MediaValues* media_values = MediaValuesCached::Create(data);

  MediaQueryEvaluator media_query_evaluator(*media_values);
  TestMQEvaluator(g_float_viewport_test_cases, media_query_evaluator);
}

TEST(MediaQueryEvaluatorTest, CachedFloatViewportNonFloatFriendly) {
  MediaValuesCached::MediaValuesCachedData data;
  data.viewport_width = 821;
  data.viewport_height = 821;
  MediaValues* media_values = MediaValuesCached::Create(data);

  MediaQueryEvaluator media_query_evaluator(*media_values);
  TestMQEvaluator(g_float_non_friendly_viewport_test_cases,
                  media_query_evaluator);
}

TEST(MediaQueryEvaluatorTest, InitialViewport) {
  std::unique_ptr<DummyPageHolder> page_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  page_holder->GetFrameView().SetMediaType(MediaTypeNames::screen);
  page_holder->GetFrameView().SetLayoutSizeFixedToFrameSize(false);
  page_holder->GetFrameView().SetInitialViewportSize(IntSize(500, 500));
  page_holder->GetFrameView().SetLayoutSize(IntSize(800, 800));
  page_holder->GetFrameView().SetFrameRect(IntRect(0, 0, 800, 800));

  MediaQueryEvaluator media_query_evaluator(
      MediaValuesInitialViewport::Create(page_holder->GetFrame()));
  TestMQEvaluator(g_viewport_test_cases, media_query_evaluator);
}

}  // namespace blink
