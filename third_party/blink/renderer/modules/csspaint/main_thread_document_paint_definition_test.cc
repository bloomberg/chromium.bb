// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/main_thread_document_paint_definition.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_syntax_descriptor.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d_settings.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TEST(MainThreadDocumentPaintDefinitionTest, NativeInvalidationProperties) {
  V8TestingScope scope;
  Vector<CSSPropertyID> native_invalidation_properties = {
      CSSPropertyID::kColor,
      CSSPropertyID::kZoom,
      CSSPropertyID::kTop,
  };
  Vector<AtomicString> custom_invalidation_properties;
  Vector<CSSSyntaxDescriptor> input_argument_types;
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  context_settings->setAlpha(true);
  CSSPaintDefinition* definition = MakeGarbageCollected<CSSPaintDefinition>(
      scope.GetScriptState(), nullptr, nullptr, native_invalidation_properties,
      custom_invalidation_properties, input_argument_types, context_settings);

  MainThreadDocumentPaintDefinition document_definition(definition);
  EXPECT_EQ(document_definition.NativeInvalidationProperties().size(), 3u);
  for (size_t i = 0; i < 3; i++) {
    EXPECT_EQ(native_invalidation_properties[i],
              document_definition.NativeInvalidationProperties()[i]);
  }
}

TEST(MainThreadDocumentPaintDefinitionTest, CustomInvalidationProperties) {
  V8TestingScope scope;
  Vector<CSSPropertyID> native_invalidation_properties;
  Vector<AtomicString> custom_invalidation_properties = {
      "--my-property",
      "--another-property",
  };
  Vector<CSSSyntaxDescriptor> input_argument_types;
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  context_settings->setAlpha(true);
  CSSPaintDefinition* definition = MakeGarbageCollected<CSSPaintDefinition>(
      scope.GetScriptState(), /*constructor=*/nullptr, /*paint=*/nullptr,
      native_invalidation_properties, custom_invalidation_properties,
      input_argument_types, context_settings);

  MainThreadDocumentPaintDefinition document_definition(definition);
  EXPECT_EQ(document_definition.CustomInvalidationProperties().size(), 2u);
  // Since the native_invalidation_properties are AtomicStrings,
  // MainThreadDocumentPaintDefinition needs to copy them to make it safe to be
  // sent to another thread.
  for (size_t i = 0; i < 2; i++) {
    EXPECT_TRUE(document_definition.CustomInvalidationProperties()[i]
                    .IsSafeToSendToAnotherThread());
    EXPECT_EQ(custom_invalidation_properties[i],
              document_definition.CustomInvalidationProperties()[i]);
  }
}

TEST(MainThreadDocumentPaintDefinitionTest, Alpha) {
  V8TestingScope scope;
  Vector<CSSPropertyID> native_invalidation_properties;
  Vector<AtomicString> custom_invalidation_properties;
  Vector<CSSSyntaxDescriptor> input_argument_types;
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  context_settings->setAlpha(true);
  CSSPaintDefinition* definition = MakeGarbageCollected<CSSPaintDefinition>(
      scope.GetScriptState(), /*constructor=*/nullptr, /*paint=*/nullptr,
      native_invalidation_properties, custom_invalidation_properties,
      input_argument_types, context_settings);

  MainThreadDocumentPaintDefinition document_definition_with_alpha(definition);
  context_settings->setAlpha(false);
  MainThreadDocumentPaintDefinition document_definition_without_alpha(
      definition);

  EXPECT_TRUE(document_definition_with_alpha.alpha());
  EXPECT_FALSE(document_definition_without_alpha.alpha());
}

}  // namespace blink
