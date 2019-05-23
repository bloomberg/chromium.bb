// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_paint_value.h"

#include <memory>
#include "base/auto_reset.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_descriptor.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/paint_generated_image.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::Values;

namespace blink {
namespace {

using CSSPaintValueTest = RenderingTest;

class MockCSSPaintImageGenerator : public CSSPaintImageGenerator {
 public:
  MOCK_METHOD3(Paint,
               scoped_refptr<Image>(const ImageResourceObserver&,
                                    const FloatSize& container_size,
                                    const CSSStyleValueVector*));
  MOCK_CONST_METHOD0(NativeInvalidationProperties, Vector<CSSPropertyID>&());
  MOCK_CONST_METHOD0(CustomInvalidationProperties, Vector<AtomicString>&());
  MOCK_CONST_METHOD0(HasAlpha, bool());
  MOCK_CONST_METHOD0(InputArgumentTypes, Vector<CSSSyntaxDescriptor>&());
  MOCK_CONST_METHOD0(IsImageGeneratorReady, bool());
  MOCK_CONST_METHOD0(WorkletId, int());
};

// CSSPaintImageGenerator requires that CSSPaintImageGeneratorCreateFunction be
// a static method. As such, it cannot access a class member and so instead we
// store a pointer to the overriding generator globally.
MockCSSPaintImageGenerator* g_override_generator = nullptr;
CSSPaintImageGenerator* ProvideOverrideGenerator(
    const String&,
    const Document&,
    CSSPaintImageGenerator::Observer*) {
  return g_override_generator;
}
}  // namespace

TEST_F(CSSPaintValueTest, DelayPaintUntilGeneratorReady) {
  NiceMock<MockCSSPaintImageGenerator>* mock_generator =
      MakeGarbageCollected<NiceMock<MockCSSPaintImageGenerator>>();
  base::AutoReset<MockCSSPaintImageGenerator*> scoped_override_generator(
      &g_override_generator, mock_generator);
  base::AutoReset<CSSPaintImageGenerator::CSSPaintImageGeneratorCreateFunction>
      scoped_create_function(
          CSSPaintImageGenerator::GetCreateFunctionForTesting(),
          ProvideOverrideGenerator);

  Vector<CSSSyntaxDescriptor> input_argument_types;
  ON_CALL(*mock_generator, InputArgumentTypes())
      .WillByDefault(ReturnRef(input_argument_types));
  const FloatSize target_size(100, 100);
  ON_CALL(*mock_generator, Paint(_, _, _))
      .WillByDefault(Return(PaintGeneratedImage::Create(nullptr, target_size)));

  SetBodyInnerHTML(R"HTML(
    <div id="target"></div>
  )HTML");
  LayoutObject* target = GetLayoutObjectByElementId("target");
  const ComputedStyle& style = *target->Style();

  auto* ident = MakeGarbageCollected<CSSCustomIdentValue>("testpainter");
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident);

  // Initially the generator is not ready, so GetImage should fail.
  EXPECT_FALSE(
      paint_value->GetImage(*target, GetDocument(), style, target_size));

  // Now mark the generator as ready - GetImage should then succeed.
  EXPECT_CALL(*mock_generator, IsImageGeneratorReady()).WillOnce(Return(true));
  EXPECT_TRUE(
      paint_value->GetImage(*target, GetDocument(), style, target_size));
}

// Regression test for https://crbug.com/835589.
TEST_F(CSSPaintValueTest, DoNotPaintForLink) {
  SetBodyInnerHTML(R"HTML(
    <style>
      a {
        background-image: paint(linkpainter);
        width: 100px;
        height: 100px;
      }
    </style>
    <a href="http://www.example.com" id="target"></a>
  )HTML");
  LayoutObject* target = GetLayoutObjectByElementId("target");
  const ComputedStyle& style = *target->Style();
  ASSERT_NE(style.InsideLink(), EInsideLink::kNotInsideLink);

  auto* ident = MakeGarbageCollected<CSSCustomIdentValue>("linkpainter");
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident);
  EXPECT_FALSE(paint_value->GetImage(*target, GetDocument(), style,
                                     FloatSize(100, 100)));
}

// Regression test for https://crbug.com/835589.
TEST_F(CSSPaintValueTest, DoNotPaintWhenAncestorHasLink) {
  SetBodyInnerHTML(R"HTML(
    <style>
      a {
        width: 200px;
        height: 200px;
      }
      b {
        background-image: paint(linkpainter);
        width: 100px;
        height: 100px;
      }
    </style>
    <a href="http://www.example.com" id="ancestor">
      <b id="target"></b>
    </a>
  )HTML");
  LayoutObject* target = GetLayoutObjectByElementId("target");
  const ComputedStyle& style = *target->Style();
  ASSERT_NE(style.InsideLink(), EInsideLink::kNotInsideLink);

  auto* ident = MakeGarbageCollected<CSSCustomIdentValue>("linkpainter");
  CSSPaintValue* paint_value = MakeGarbageCollected<CSSPaintValue>(ident);
  EXPECT_FALSE(paint_value->GetImage(*target, GetDocument(), style,
                                     FloatSize(100, 100)));
}

}  // namespace blink
