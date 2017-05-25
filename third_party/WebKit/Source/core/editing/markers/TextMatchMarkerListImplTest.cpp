// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/TextMatchMarkerListImpl.h"

#include "core/editing/EditingTestBase.h"

namespace blink {

class TextMatchMarkerListImplTest : public EditingTestBase {
 protected:
  TextMatchMarkerListImplTest() : marker_list_(new TextMatchMarkerListImpl()) {}

  DocumentMarker* CreateMarker(unsigned start_offset, unsigned end_offset) {
    return new DocumentMarker(start_offset, end_offset,
                              DocumentMarker::MatchStatus::kInactive);
  }

  Persistent<TextMatchMarkerListImpl> marker_list_;
};

TEST_F(TextMatchMarkerListImplTest, MarkerType) {
  EXPECT_EQ(DocumentMarker::kTextMatch, marker_list_->MarkerType());
}

TEST_F(TextMatchMarkerListImplTest, Add) {
  EXPECT_EQ(0u, marker_list_->GetMarkers().size());

  marker_list_->Add(CreateMarker(0, 1));
  marker_list_->Add(CreateMarker(1, 2));

  EXPECT_EQ(2u, marker_list_->GetMarkers().size());

  EXPECT_EQ(0u, marker_list_->GetMarkers()[0]->StartOffset());
  EXPECT_EQ(1u, marker_list_->GetMarkers()[0]->EndOffset());

  EXPECT_EQ(1u, marker_list_->GetMarkers()[1]->StartOffset());
  EXPECT_EQ(2u, marker_list_->GetMarkers()[1]->EndOffset());
}

}  // namespace blink
