// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/ActiveSuggestionMarkerListImpl.h"

#include "core/editing/markers/ActiveSuggestionMarker.h"
#include "core/editing/testing/EditingTestBase.h"

namespace blink {

class ActiveSuggestionMarkerListImplTest : public EditingTestBase {
 protected:
  ActiveSuggestionMarkerListImplTest()
      : marker_list_(new ActiveSuggestionMarkerListImpl()) {}

  DocumentMarker* CreateMarker(unsigned start_offset, unsigned end_offset) {
    return new ActiveSuggestionMarker(
        start_offset, end_offset, Color::kTransparent,
        ui::mojom::ImeTextSpanThickness::kThin, Color::kBlack);
  }

  Persistent<ActiveSuggestionMarkerListImpl> marker_list_;
};

// ActiveSuggestionMarkerListImpl shouldn't merge markers with touching
// endpoints
TEST_F(ActiveSuggestionMarkerListImplTest, Add) {
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
