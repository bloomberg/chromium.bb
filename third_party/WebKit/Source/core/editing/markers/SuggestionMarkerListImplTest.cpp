// Copyright 2017 The Chromium Authors. All rights reserved.  Use of
// this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/SuggestionMarkerListImpl.h"

#include "core/editing/markers/SuggestionMarker.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class SuggestionMarkerListImplTest : public ::testing::Test {
 protected:
  SuggestionMarkerListImplTest()
      : marker_list_(new SuggestionMarkerListImpl()) {}

  DocumentMarker* CreateMarker(unsigned start_offset, unsigned end_offset) {
    return new SuggestionMarker(start_offset, end_offset, Vector<String>(),
                                Color::kTransparent, Color::kTransparent,
                                StyleableMarker::Thickness::kThin,
                                Color::kTransparent);
  }

  Persistent<SuggestionMarkerListImpl> marker_list_;
};

namespace {

bool compare_markers(const Member<DocumentMarker>& marker1,
                     const Member<DocumentMarker>& marker2) {
  if (marker1->StartOffset() != marker2->StartOffset())
    return marker1->StartOffset() < marker2->StartOffset();

  return marker1->EndOffset() < marker2->EndOffset();
}
}  // namespace

TEST_F(SuggestionMarkerListImplTest, MarkerType) {
  EXPECT_EQ(DocumentMarker::kSuggestion, marker_list_->MarkerType());
}

TEST_F(SuggestionMarkerListImplTest, AddOverlapping) {
  // Add some overlapping markers in an arbitrary order and verify that the
  // list stores them properly
  marker_list_->Add(CreateMarker(40, 50));
  marker_list_->Add(CreateMarker(10, 40));
  marker_list_->Add(CreateMarker(20, 50));
  marker_list_->Add(CreateMarker(10, 30));
  marker_list_->Add(CreateMarker(10, 50));
  marker_list_->Add(CreateMarker(30, 50));
  marker_list_->Add(CreateMarker(30, 40));
  marker_list_->Add(CreateMarker(10, 20));
  marker_list_->Add(CreateMarker(20, 40));
  marker_list_->Add(CreateMarker(20, 30));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  std::sort(markers.begin(), markers.end(), compare_markers);

  EXPECT_EQ(10u, markers.size());

  EXPECT_EQ(10u, markers[0]->StartOffset());
  EXPECT_EQ(20u, markers[0]->EndOffset());

  EXPECT_EQ(10u, markers[1]->StartOffset());
  EXPECT_EQ(30u, markers[1]->EndOffset());

  EXPECT_EQ(10u, markers[2]->StartOffset());
  EXPECT_EQ(40u, markers[2]->EndOffset());

  EXPECT_EQ(10u, markers[3]->StartOffset());
  EXPECT_EQ(50u, markers[3]->EndOffset());

  EXPECT_EQ(20u, markers[4]->StartOffset());
  EXPECT_EQ(30u, markers[4]->EndOffset());

  EXPECT_EQ(20u, markers[5]->StartOffset());
  EXPECT_EQ(40u, markers[5]->EndOffset());

  EXPECT_EQ(20u, markers[6]->StartOffset());
  EXPECT_EQ(50u, markers[6]->EndOffset());

  EXPECT_EQ(30u, markers[7]->StartOffset());
  EXPECT_EQ(40u, markers[7]->EndOffset());

  EXPECT_EQ(30u, markers[8]->StartOffset());
  EXPECT_EQ(50u, markers[8]->EndOffset());

  EXPECT_EQ(40u, markers[9]->StartOffset());
  EXPECT_EQ(50u, markers[9]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest, FirstMarkerIntersectingRange_Empty) {
  DocumentMarker* marker = marker_list_->FirstMarkerIntersectingRange(0, 10);
  EXPECT_EQ(nullptr, marker);
}

TEST_F(SuggestionMarkerListImplTest,
       FirstMarkerIntersectingRange_TouchingStart) {
  marker_list_->Add(CreateMarker(1, 10));
  marker_list_->Add(CreateMarker(0, 10));

  DocumentMarker* marker = marker_list_->FirstMarkerIntersectingRange(0, 1);

  EXPECT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest, FirstMarkerIntersectingRange_TouchingEnd) {
  marker_list_->Add(CreateMarker(0, 9));
  marker_list_->Add(CreateMarker(0, 10));

  DocumentMarker* marker = marker_list_->FirstMarkerIntersectingRange(9, 10);

  EXPECT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest, MarkersIntersectingRange_TouchingStart) {
  marker_list_->Add(CreateMarker(0, 9));
  marker_list_->Add(CreateMarker(1, 9));
  marker_list_->Add(CreateMarker(0, 10));
  marker_list_->Add(CreateMarker(1, 10));

  DocumentMarkerVector markers_intersecting_range =
      marker_list_->MarkersIntersectingRange(0, 1);

  EXPECT_EQ(2u, markers_intersecting_range.size());

  EXPECT_EQ(0u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(9u, markers_intersecting_range[0]->EndOffset());

  EXPECT_EQ(0u, markers_intersecting_range[1]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[1]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest, MarkersIntersectingRange_TouchingEnd) {
  marker_list_->Add(CreateMarker(0, 9));
  marker_list_->Add(CreateMarker(1, 9));
  marker_list_->Add(CreateMarker(0, 10));
  marker_list_->Add(CreateMarker(1, 10));

  DocumentMarkerVector markers_intersecting_range =
      marker_list_->MarkersIntersectingRange(9, 10);

  EXPECT_EQ(2u, markers_intersecting_range.size());

  EXPECT_EQ(0u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());

  EXPECT_EQ(1u, markers_intersecting_range[1]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[1]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest, MoveMarkers) {
  marker_list_->Add(CreateMarker(30, 40));
  marker_list_->Add(CreateMarker(0, 30));
  marker_list_->Add(CreateMarker(10, 40));
  marker_list_->Add(CreateMarker(0, 20));
  marker_list_->Add(CreateMarker(0, 40));
  marker_list_->Add(CreateMarker(20, 40));
  marker_list_->Add(CreateMarker(20, 30));
  marker_list_->Add(CreateMarker(0, 10));
  marker_list_->Add(CreateMarker(10, 30));
  marker_list_->Add(CreateMarker(10, 20));
  marker_list_->Add(CreateMarker(11, 21));

  DocumentMarkerList* dst_list = new SuggestionMarkerListImpl();
  // The markers with start and end offset < 11 should be moved to dst_list.
  // Markers that start before 11 and end at 11 or later should be removed.
  // Markers that start at 11 or later should not be moved.
  marker_list_->MoveMarkers(11, dst_list);

  DocumentMarkerVector source_list_markers = marker_list_->GetMarkers();
  std::sort(source_list_markers.begin(), source_list_markers.end(),
            compare_markers);

  EXPECT_EQ(4u, source_list_markers.size());

  EXPECT_EQ(11u, source_list_markers[0]->StartOffset());
  EXPECT_EQ(21u, source_list_markers[0]->EndOffset());

  EXPECT_EQ(20u, source_list_markers[1]->StartOffset());
  EXPECT_EQ(30u, source_list_markers[1]->EndOffset());

  EXPECT_EQ(20u, source_list_markers[2]->StartOffset());
  EXPECT_EQ(40u, source_list_markers[2]->EndOffset());

  EXPECT_EQ(30u, source_list_markers[3]->StartOffset());
  EXPECT_EQ(40u, source_list_markers[3]->EndOffset());

  DocumentMarkerVector dst_list_markers = dst_list->GetMarkers();
  std::sort(dst_list_markers.begin(), dst_list_markers.end(), compare_markers);

  // Markers
  EXPECT_EQ(1u, dst_list_markers.size());

  EXPECT_EQ(0u, dst_list_markers[0]->StartOffset());
  EXPECT_EQ(10u, dst_list_markers[0]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest, RemoveMarkersEmptyList) {
  EXPECT_FALSE(marker_list_->RemoveMarkers(0, 10));
  EXPECT_EQ(0u, marker_list_->GetMarkers().size());
}

TEST_F(SuggestionMarkerListImplTest, RemoveMarkersTouchingEndpoints) {
  marker_list_->Add(CreateMarker(30, 40));
  marker_list_->Add(CreateMarker(40, 50));
  marker_list_->Add(CreateMarker(10, 20));
  marker_list_->Add(CreateMarker(0, 10));
  marker_list_->Add(CreateMarker(20, 30));

  EXPECT_TRUE(marker_list_->RemoveMarkers(20, 10));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  std::sort(markers.begin(), markers.end(), compare_markers);

  EXPECT_EQ(4u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  EXPECT_EQ(10u, markers[1]->StartOffset());
  EXPECT_EQ(20u, markers[1]->EndOffset());

  EXPECT_EQ(30u, markers[2]->StartOffset());
  EXPECT_EQ(40u, markers[2]->EndOffset());

  EXPECT_EQ(40u, markers[3]->StartOffset());
  EXPECT_EQ(50u, markers[3]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest, RemoveMarkersOneCharacterIntoInterior) {
  marker_list_->Add(CreateMarker(30, 40));
  marker_list_->Add(CreateMarker(40, 50));
  marker_list_->Add(CreateMarker(10, 20));
  marker_list_->Add(CreateMarker(0, 10));
  marker_list_->Add(CreateMarker(20, 30));

  EXPECT_TRUE(marker_list_->RemoveMarkers(19, 12));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  std::sort(markers.begin(), markers.end(), compare_markers);

  EXPECT_EQ(2u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  EXPECT_EQ(40u, markers[1]->StartOffset());
  EXPECT_EQ(50u, markers[1]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest, ShiftMarkers) {
  marker_list_->Add(CreateMarker(30, 40));
  marker_list_->Add(CreateMarker(40, 50));
  marker_list_->Add(CreateMarker(10, 20));
  marker_list_->Add(CreateMarker(0, 10));
  marker_list_->Add(CreateMarker(20, 30));

  EXPECT_TRUE(marker_list_->ShiftMarkers(15, 20, 0));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  std::sort(markers.begin(), markers.end(), compare_markers);

  EXPECT_EQ(4u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  EXPECT_EQ(10u, markers[1]->StartOffset());
  EXPECT_EQ(15u, markers[1]->EndOffset());

  EXPECT_EQ(15u, markers[2]->StartOffset());
  EXPECT_EQ(20u, markers[2]->EndOffset());

  EXPECT_EQ(20u, markers[3]->StartOffset());
  EXPECT_EQ(30u, markers[3]->EndOffset());
}

}  // namespace blink
