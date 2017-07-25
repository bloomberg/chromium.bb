// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaQueryList.h"

#include "core/css/MediaList.h"
#include "core/css/MediaQueryListListener.h"
#include "core/css/MediaQueryMatcher.h"
#include "core/dom/Document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class TestListener : public MediaQueryListListener {
 public:
  void NotifyMediaQueryChanged() override {}
};

}  // anonymous namespace

TEST(MediaQueryListTest, CrashInStop) {
  Document* document = Document::CreateForTest();
  MediaQueryList* list = MediaQueryList::Create(
      document, MediaQueryMatcher::Create(*document), MediaQuerySet::Create());
  list->AddListener(new TestListener());
  list->ContextDestroyed(document);
  // This test passes if it's not crashed.
}

}  // namespace blink
