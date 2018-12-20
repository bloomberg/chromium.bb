// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/paint_worklet_style_property_map.h"

#include <memory>
#include "base/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/waitable_event.h"
#include "third_party/blink/renderer/platform/web_thread_supporting_gc.h"

namespace blink {

class PaintWorkletStylePropertyMapTest : public PageTestBase {
 public:
  PaintWorkletStylePropertyMapTest() = default;

  void SetUp() override { PageTestBase::SetUp(IntSize()); }

  Node* PageNode() { return GetDocument().documentElement(); }

  static void CheckCustomProperties(
      PaintWorkletStylePropertyMap* map,
      DummyExceptionStateForTesting& exception_state) {
    const CSSStyleValue* foo = map->get(nullptr, "--foo", exception_state);
    ASSERT_NE(nullptr, foo);
    ASSERT_EQ(CSSStyleValue::kUnknownType, foo->GetType());
    EXPECT_FALSE(exception_state.HadException());

    EXPECT_EQ(true, map->has(nullptr, "--foo", exception_state));
    EXPECT_FALSE(exception_state.HadException());

    CSSStyleValueVector fooAll = map->getAll(nullptr, "--foo", exception_state);
    EXPECT_EQ(1U, fooAll.size());
    ASSERT_NE(nullptr, fooAll[0]);
    ASSERT_EQ(CSSStyleValue::kUnknownType, fooAll[0]->GetType());
    EXPECT_FALSE(exception_state.HadException());

    EXPECT_EQ(nullptr, map->get(nullptr, "--quix", exception_state));
    EXPECT_FALSE(exception_state.HadException());

    EXPECT_EQ(false, map->has(nullptr, "--quix", exception_state));
    EXPECT_FALSE(exception_state.HadException());

    EXPECT_EQ(CSSStyleValueVector(),
              map->getAll(nullptr, "--quix", exception_state));
    EXPECT_FALSE(exception_state.HadException());
  }

  static void CheckNativeProperties(
      PaintWorkletStylePropertyMap* map,
      DummyExceptionStateForTesting& exception_state) {
    map->get(nullptr, "color", exception_state);
    EXPECT_FALSE(exception_state.HadException());

    map->has(nullptr, "color", exception_state);
    EXPECT_FALSE(exception_state.HadException());

    map->getAll(nullptr, "color", exception_state);
    EXPECT_FALSE(exception_state.HadException());

    map->get(nullptr, "align-contents", exception_state);
    EXPECT_TRUE(exception_state.HadException());
    exception_state.ClearException();

    map->has(nullptr, "align-contents", exception_state);
    EXPECT_TRUE(exception_state.HadException());
    exception_state.ClearException();

    map->getAll(nullptr, "align-contents", exception_state);
    EXPECT_TRUE(exception_state.HadException());
    exception_state.ClearException();
  }

  static void CheckStyleMap(WaitableEvent* waitable_event,
                            scoped_refptr<PaintWorkletInput> input) {
    DCHECK(!IsMainThread());

    PaintWorkletStylePropertyMap* map = input->StyleMap();
    DCHECK(map);

    HashMap<String, String> values = map->ValuesForTest();
    EXPECT_EQ(values.size(), 4u);
    EXPECT_EQ(values.at("color"), "rgb(0, 255, 0)");
    EXPECT_EQ(values.at("align-items"), "normal");
    EXPECT_EQ(values.at("--foo"), "PaintWorklet");
    EXPECT_EQ(values.at("--bar"), "");

    waitable_event->Signal();
  }
};

TEST_F(PaintWorkletStylePropertyMapTest, NativePropertyAccessors) {
  Vector<CSSPropertyID> native_properties(
      {CSSPropertyColor, CSSPropertyAlignItems, CSSPropertyBackground});
  Vector<AtomicString> empty_custom_properties;
  GetDocument().documentElement()->style()->setProperty(
      &GetDocument(), "color", "rgb(0, 255, 0)", "", ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesForTest();
  Node* node = PageNode();

  PaintWorkletStylePropertyMap* map =
      MakeGarbageCollected<PaintWorkletStylePropertyMap>(
          GetDocument(), node->ComputedStyleRef(), node, native_properties,
          empty_custom_properties);

  HashMap<String, String> values = map->ValuesForTest();
  EXPECT_EQ(values.size(), 2u);
  EXPECT_EQ(values.at("color"), "rgb(0, 255, 0)");
  EXPECT_EQ(values.at("align-items"), "normal");

  DummyExceptionStateForTesting exception_state;
  CheckNativeProperties(map, exception_state);
}

TEST_F(PaintWorkletStylePropertyMapTest, CustomPropertyAccessors) {
  Vector<CSSPropertyID> empty_native_properties;
  Vector<AtomicString> custom_properties({"--foo", "--bar"});
  GetDocument().documentElement()->style()->setProperty(
      &GetDocument(), "--foo", "PaintWorklet", "", ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesForTest();
  Node* node = PageNode();

  PaintWorkletStylePropertyMap* map =
      MakeGarbageCollected<PaintWorkletStylePropertyMap>(
          GetDocument(), node->ComputedStyleRef(), node,
          empty_native_properties, custom_properties);

  HashMap<String, String> values = map->ValuesForTest();
  EXPECT_EQ(values.size(), 2u);
  EXPECT_EQ(values.at("--foo"), "PaintWorklet");
  EXPECT_EQ(values.at("--bar"), "");

  DummyExceptionStateForTesting exception_state;
  CheckCustomProperties(map, exception_state);
}

// This test ensures that Blink::PaintWorkletInput can be safely passed cross
// threads and no information is lost.
TEST_F(PaintWorkletStylePropertyMapTest, PassValuesCrossThreads) {
  Vector<CSSPropertyID> native_properties(
      {CSSPropertyColor, CSSPropertyAlignItems});
  GetDocument().documentElement()->style()->setProperty(
      &GetDocument(), "color", "rgb(0, 255, 0)", "", ASSERT_NO_EXCEPTION);
  Vector<AtomicString> custom_properties({"--foo", "--bar"});
  GetDocument().documentElement()->style()->setProperty(
      &GetDocument(), "--foo", "PaintWorklet", "", ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesForTest();
  Node* node = PageNode();

  scoped_refptr<PaintWorkletInput> input =
      base::MakeRefCounted<PaintWorkletInput>(
          "test", FloatSize(100, 100), 1.0f, GetDocument(),
          node->ComputedStyleRef(), node, native_properties, custom_properties);
  DCHECK(input);

  // TODO(xidachen): this isn't enough, find a right thread such that we can do
  // CheckNative/CustomProperties on that thread.
  std::unique_ptr<WebThreadSupportingGC> thread = WebThreadSupportingGC::Create(
      ThreadCreationParams(WebThreadType::kTestThread));
  WaitableEvent waitable_event;
  thread->PostTask(
      FROM_HERE, CrossThreadBind(
                     &PaintWorkletStylePropertyMapTest::CheckStyleMap,
                     CrossThreadUnretained(&waitable_event), std::move(input)));
  waitable_event.Wait();
}

}  // namespace blink
