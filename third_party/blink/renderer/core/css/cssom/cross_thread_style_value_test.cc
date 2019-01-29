// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/cross_thread_style_value.h"

#include <memory>
#include "base/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unsupported_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/waitable_event.h"
#include "third_party/blink/renderer/platform/web_thread_supporting_gc.h"

namespace blink {

class CrossThreadStyleValueTest : public testing::Test {
 public:
  void ShutDown(WaitableEvent* waitable_event) {
    DCHECK(!IsMainThread());
    thread_->ShutdownOnThread();
    waitable_event->Signal();
  }

  void ShutDownThread() {
    WaitableEvent waitable_event;
    thread_->PostTask(FROM_HERE,
                      CrossThreadBind(&CrossThreadStyleValueTest::ShutDown,
                                      CrossThreadUnretained(this),
                                      CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  void CheckUnsupportedValue(
      WaitableEvent* waitable_event,
      std::unique_ptr<CrossThreadUnsupportedValue> value) {
    DCHECK(!IsMainThread());
    thread_->InitializeOnThread();

    EXPECT_EQ(value->value_, "Unsupported");
    waitable_event->Signal();
  }

  void CheckKeywordValue(WaitableEvent* waitable_event,
                         std::unique_ptr<CrossThreadKeywordValue> value) {
    DCHECK(!IsMainThread());
    thread_->InitializeOnThread();

    EXPECT_EQ(value->keyword_value_, "Keyword");
    waitable_event->Signal();
  }

  void CheckUnitValue(WaitableEvent* waitable_event,
                      std::unique_ptr<CrossThreadUnitValue> value) {
    DCHECK(!IsMainThread());
    thread_->InitializeOnThread();

    EXPECT_EQ(value->value_, 1);
    EXPECT_EQ(value->unit_, CSSPrimitiveValue::UnitType::kDegrees);
    waitable_event->Signal();
  }

 protected:
  std::unique_ptr<WebThreadSupportingGC> thread_;
};

// Ensure that a CrossThreadUnsupportedValue can be safely passed cross
// threads.
TEST_F(CrossThreadStyleValueTest, PassUnsupportedValueCrossThread) {
  std::unique_ptr<CrossThreadUnsupportedValue> value =
      std::make_unique<CrossThreadUnsupportedValue>("Unsupported");
  DCHECK(value);

  // Use a WebThreadSupportingGC to emulate worklet thread.
  thread_ = WebThreadSupportingGC::Create(
      ThreadCreationParams(WebThreadType::kTestThread));
  WaitableEvent waitable_event;
  thread_->PostTask(
      FROM_HERE,
      CrossThreadBind(&CrossThreadStyleValueTest::CheckUnsupportedValue,
                      CrossThreadUnretained(this),
                      CrossThreadUnretained(&waitable_event),
                      WTF::Passed(std::move(value))));
  waitable_event.Wait();

  ShutDownThread();
}

TEST_F(CrossThreadStyleValueTest, CrossThreadUnsupportedValueToCSSStyleValue) {
  std::unique_ptr<CrossThreadUnsupportedValue> value =
      std::make_unique<CrossThreadUnsupportedValue>("Unsupported");
  DCHECK(value);

  const CSSStyleValue* const style_value = value->ToCSSStyleValue();
  EXPECT_EQ(style_value->GetType(),
            CSSStyleValue::StyleValueType::kUnknownType);
  EXPECT_EQ(style_value->CSSText(), "Unsupported");
}

TEST_F(CrossThreadStyleValueTest, PassKeywordValueCrossThread) {
  std::unique_ptr<CrossThreadKeywordValue> value =
      std::make_unique<CrossThreadKeywordValue>("Keyword");
  DCHECK(value);

  // Use a WebThreadSupportingGC to emulate worklet thread.
  thread_ = WebThreadSupportingGC::Create(
      ThreadCreationParams(WebThreadType::kTestThread));
  WaitableEvent waitable_event;
  thread_->PostTask(
      FROM_HERE, CrossThreadBind(&CrossThreadStyleValueTest::CheckKeywordValue,
                                 CrossThreadUnretained(this),
                                 CrossThreadUnretained(&waitable_event),
                                 WTF::Passed(std::move(value))));
  waitable_event.Wait();

  ShutDownThread();
}

TEST_F(CrossThreadStyleValueTest, CrossThreadKeywordValueToCSSStyleValue) {
  std::unique_ptr<CrossThreadKeywordValue> value =
      std::make_unique<CrossThreadKeywordValue>("Keyword");
  DCHECK(value);

  CSSStyleValue* style_value = value->ToCSSStyleValue();
  EXPECT_EQ(style_value->GetType(),
            CSSStyleValue::StyleValueType::kKeywordType);
  EXPECT_EQ(static_cast<CSSKeywordValue*>(style_value)->value(), "Keyword");
}

TEST_F(CrossThreadStyleValueTest, PassUnitValueCrossThread) {
  std::unique_ptr<CrossThreadUnitValue> value =
      std::make_unique<CrossThreadUnitValue>(
          1, CSSPrimitiveValue::UnitType::kDegrees);
  DCHECK(value);

  // Use a WebThreadSupportingGC to emulate worklet thread.
  thread_ = WebThreadSupportingGC::Create(
      ThreadCreationParams(WebThreadType::kTestThread));
  WaitableEvent waitable_event;
  thread_->PostTask(FROM_HERE,
                    CrossThreadBind(&CrossThreadStyleValueTest::CheckUnitValue,
                                    CrossThreadUnretained(this),
                                    CrossThreadUnretained(&waitable_event),
                                    WTF::Passed(std::move(value))));
  waitable_event.Wait();

  ShutDownThread();
}

TEST_F(CrossThreadStyleValueTest, CrossThreadUnitValueToCSSStyleValue) {
  std::unique_ptr<CrossThreadUnitValue> value =
      std::make_unique<CrossThreadUnitValue>(
          1, CSSPrimitiveValue::UnitType::kDegrees);
  DCHECK(value);

  CSSStyleValue* style_value = value->ToCSSStyleValue();
  EXPECT_EQ(style_value->GetType(), CSSStyleValue::StyleValueType::kUnitType);
  EXPECT_EQ(static_cast<CSSUnitValue*>(style_value)->value(), 1);
  EXPECT_EQ(static_cast<CSSUnitValue*>(style_value)->unit(), "deg");
}

}  // namespace blink
