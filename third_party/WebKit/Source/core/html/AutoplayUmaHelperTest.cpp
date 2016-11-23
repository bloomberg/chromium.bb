// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/AutoplayUmaHelper.h"

#include "core/dom/Document.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/testing/DummyPageHolder.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using ::testing::Invoke;

class MockAutoplayUmaHelper : public AutoplayUmaHelper {
 public:
  MockAutoplayUmaHelper(HTMLMediaElement* element)
      : AutoplayUmaHelper(element) {
    ON_CALL(*this, handleContextDestroyed())
        .WillByDefault(
            Invoke(this, &MockAutoplayUmaHelper::reallyHandleContextDestroyed));
  }

  void handlePlayingEvent() { AutoplayUmaHelper::handlePlayingEvent(); }

  MOCK_METHOD0(handleContextDestroyed, void());

  // Making this a wrapper function to avoid calling the mocked version.
  void reallyHandleContextDestroyed() {
    AutoplayUmaHelper::handleContextDestroyed();
  }
};

class AutoplayUmaHelperTest : public testing::Test {
 protected:
  Document& document() { return m_pageHolder->document(); }

  HTMLMediaElement& mediaElement() {
    Element* element = document().getElementById("video");
    DCHECK(element);
    return toHTMLVideoElement(*element);
  }

  MockAutoplayUmaHelper& umaHelper() {
    return *(static_cast<MockAutoplayUmaHelper*>(
        mediaElement().m_autoplayUmaHelper.get()));
  }

  std::unique_ptr<DummyPageHolder>& pageHolder() { return m_pageHolder; }

  MOCK_METHOD0(TestEnded, void());

 private:
  void SetUp() override {
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
    document().documentElement()->setInnerHTML("<video id=video></video>",
                                               ASSERT_NO_EXCEPTION);
    HTMLMediaElement& element = mediaElement();
    element.m_autoplayUmaHelper = new MockAutoplayUmaHelper(&element);
  }

  std::unique_ptr<DummyPageHolder> m_pageHolder;
};

TEST_F(AutoplayUmaHelperTest, VisibilityChangeWhenUnload) {
  // This is to avoid handleContextDestroyed() to be called during TearDown().
  EXPECT_CALL(*this, TestEnded())
      .After(EXPECT_CALL(umaHelper(), handleContextDestroyed()));

  mediaElement().setMuted(true);
  umaHelper().onAutoplayInitiated(AutoplaySource::Attribute);
  umaHelper().handlePlayingEvent();
  pageHolder().reset();
  TestEnded();
}

}  // namespace blink
