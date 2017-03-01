// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/shadow/MediaControls.h"

#include "core/dom/Document.h"
#include "core/html/HTMLVideoElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/ThreadState.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MediaControlsLeakTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
    Document& document = this->document();

    document.write("<body><video controls></video></body>");
    m_video = toHTMLVideoElement(document.querySelector("video"));
    m_controls = m_video->mediaControls();
  }

  Document& document() { return m_pageHolder->document(); }
  HTMLVideoElement* video() { return m_video; }
  MediaControls* controls() { return m_controls; }

 private:
  std::unique_ptr<DummyPageHolder> m_pageHolder;
  WeakPersistent<HTMLVideoElement> m_video;
  WeakPersistent<MediaControls> m_controls;
};

TEST_F(MediaControlsLeakTest, RemovingFromDocumentCollectsAll) {
  ASSERT_NE(video(), nullptr);
  EXPECT_TRUE(video()->hasEventListeners());
  EXPECT_NE(controls(), nullptr);
  EXPECT_TRUE(document().hasEventListeners());

  document().body()->setInnerHTML("");

  // When removed from the document, the event listeners should have been
  // dropped.
  EXPECT_FALSE(document().hasEventListeners());
  // The video element should still have some event listeners.
  EXPECT_TRUE(video()->hasEventListeners());

  ThreadState::current()->collectAllGarbage();

  // They have been GC'd.
  EXPECT_EQ(video(), nullptr);
  EXPECT_EQ(controls(), nullptr);
}

TEST_F(MediaControlsLeakTest, ReInsertingInDocumentCollectsControls) {
  ASSERT_NE(video(), nullptr);
  EXPECT_TRUE(video()->hasEventListeners());
  EXPECT_NE(controls(), nullptr);
  EXPECT_TRUE(document().hasEventListeners());

  // This should be a no-op.
  document().body()->removeChild(video());
  document().body()->appendChild(video());

  EXPECT_TRUE(document().hasEventListeners());
  EXPECT_TRUE(video()->hasEventListeners());

  ThreadState::current()->collectAllGarbage();

  ASSERT_NE(video(), nullptr);
  EXPECT_NE(controls(), nullptr);
  EXPECT_EQ(controls(), video()->mediaControls());
}

}  // namespace blink
