// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/cross_thread_style_value.h"

#include <memory>
#include "base/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unsupported_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
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

}  // namespace blink
