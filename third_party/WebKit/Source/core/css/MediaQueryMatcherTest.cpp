// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaQueryMatcher.h"

#include <memory>
#include "core/css/MediaList.h"
#include "core/media_type_names.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(MediaQueryMatcherTest, LostFrame) {
  std::unique_ptr<DummyPageHolder> page_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  MediaQueryMatcher* matcher =
      MediaQueryMatcher::Create(page_holder->GetDocument());
  RefPtr<MediaQuerySet> query_set = MediaQuerySet::Create(MediaTypeNames::all);
  ASSERT_TRUE(matcher->Evaluate(query_set.get()));

  matcher->DocumentDetached();
  ASSERT_FALSE(matcher->Evaluate(query_set.get()));
}

}  // namespace blink
