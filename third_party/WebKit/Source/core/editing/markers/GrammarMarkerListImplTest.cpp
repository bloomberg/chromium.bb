// Copyright 2017 The Chromium Authors. All rights reserved.  Use of
// this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/SpellCheckMarkerListImpl.h"

#include "core/editing/markers/GrammarMarkerListImpl.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// Functionality implemented in SpellCheckMarkerListImpl is tested in
// SpellingMarkerListImplTest.cpp.

class GrammarMarkerListImplTest : public ::testing::Test {
 protected:
  GrammarMarkerListImplTest() : marker_list_(new GrammarMarkerListImpl()) {}

  Persistent<GrammarMarkerListImpl> marker_list_;
};

// Test cases for functionality implemented by GrammarMarkerListImpl.

TEST_F(GrammarMarkerListImplTest, MarkerType) {
  EXPECT_EQ(DocumentMarker::kGrammar, marker_list_->MarkerType());
}

}  // namespace
