// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/UnsortedDocumentMarkerListEditor.h"

#include "core/editing/markers/SuggestionMarker.h"
#include "core/editing/markers/SuggestionMarkerListImpl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class UnsortedDocumentMarkerListEditorTest : public ::testing::Test {
 protected:
  DocumentMarker* CreateMarker(unsigned start_offset, unsigned end_offset) {
    return new SuggestionMarker(start_offset, end_offset, Vector<String>(),
                                Color::kTransparent, Color::kTransparent,
                                StyleableMarker::Thickness::kThin,
                                Color::kTransparent);
  }

  PersistentHeapVector<Member<DocumentMarker>> marker_list_;
};

namespace {

bool compare_markers(const Member<DocumentMarker>& marker1,
                     const Member<DocumentMarker>& marker2) {
  if (marker1->StartOffset() != marker2->StartOffset())
    return marker1->StartOffset() < marker2->StartOffset();

  return marker1->EndOffset() < marker2->EndOffset();
}

}  // namespace

TEST_F(UnsortedDocumentMarkerListEditorTest, MoveMarkers) {
  marker_list_.push_back(CreateMarker(30, 40));
  marker_list_.push_back(CreateMarker(0, 30));
  marker_list_.push_back(CreateMarker(10, 40));
  marker_list_.push_back(CreateMarker(0, 20));
  marker_list_.push_back(CreateMarker(0, 40));
  marker_list_.push_back(CreateMarker(20, 40));
  marker_list_.push_back(CreateMarker(20, 30));
  marker_list_.push_back(CreateMarker(0, 10));
  marker_list_.push_back(CreateMarker(10, 30));
  marker_list_.push_back(CreateMarker(10, 20));
  marker_list_.push_back(CreateMarker(11, 21));

  DocumentMarkerList* dst_list = new SuggestionMarkerListImpl();
  // The markers with start and end offset < 11 should be moved to dst_list.
  // Markers that start before 11 and end at 11 or later should be removed.
  // Markers that start at 11 or later should not be moved.
  UnsortedDocumentMarkerListEditor::MoveMarkers(&marker_list_, 11, dst_list);

  std::sort(marker_list_.begin(), marker_list_.end(), compare_markers);

  EXPECT_EQ(4u, marker_list_.size());

  EXPECT_EQ(11u, marker_list_[0]->StartOffset());
  EXPECT_EQ(21u, marker_list_[0]->EndOffset());

  EXPECT_EQ(20u, marker_list_[1]->StartOffset());
  EXPECT_EQ(30u, marker_list_[1]->EndOffset());

  EXPECT_EQ(20u, marker_list_[2]->StartOffset());
  EXPECT_EQ(40u, marker_list_[2]->EndOffset());

  EXPECT_EQ(30u, marker_list_[3]->StartOffset());
  EXPECT_EQ(40u, marker_list_[3]->EndOffset());

  DocumentMarkerVector dst_list_markers = dst_list->GetMarkers();
  std::sort(dst_list_markers.begin(), dst_list_markers.end(), compare_markers);

  // Markers
  EXPECT_EQ(1u, dst_list_markers.size());

  EXPECT_EQ(0u, dst_list_markers[0]->StartOffset());
  EXPECT_EQ(10u, dst_list_markers[0]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest, RemoveMarkersEmptyList) {
  EXPECT_FALSE(
      UnsortedDocumentMarkerListEditor::RemoveMarkers(&marker_list_, 0, 10));
  EXPECT_EQ(0u, marker_list_.size());
}

TEST_F(UnsortedDocumentMarkerListEditorTest, RemoveMarkersTouchingEndpoints) {
  marker_list_.push_back(CreateMarker(30, 40));
  marker_list_.push_back(CreateMarker(40, 50));
  marker_list_.push_back(CreateMarker(10, 20));
  marker_list_.push_back(CreateMarker(0, 10));
  marker_list_.push_back(CreateMarker(20, 30));

  EXPECT_TRUE(
      UnsortedDocumentMarkerListEditor::RemoveMarkers(&marker_list_, 20, 10));

  std::sort(marker_list_.begin(), marker_list_.end(), compare_markers);

  EXPECT_EQ(4u, marker_list_.size());

  EXPECT_EQ(0u, marker_list_[0]->StartOffset());
  EXPECT_EQ(10u, marker_list_[0]->EndOffset());

  EXPECT_EQ(10u, marker_list_[1]->StartOffset());
  EXPECT_EQ(20u, marker_list_[1]->EndOffset());

  EXPECT_EQ(30u, marker_list_[2]->StartOffset());
  EXPECT_EQ(40u, marker_list_[2]->EndOffset());

  EXPECT_EQ(40u, marker_list_[3]->StartOffset());
  EXPECT_EQ(50u, marker_list_[3]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       RemoveMarkersOneCharacterIntoInterior) {
  marker_list_.push_back(CreateMarker(30, 40));
  marker_list_.push_back(CreateMarker(40, 50));
  marker_list_.push_back(CreateMarker(10, 20));
  marker_list_.push_back(CreateMarker(0, 10));
  marker_list_.push_back(CreateMarker(20, 30));

  EXPECT_TRUE(
      UnsortedDocumentMarkerListEditor::RemoveMarkers(&marker_list_, 19, 12));

  std::sort(marker_list_.begin(), marker_list_.end(), compare_markers);

  EXPECT_EQ(2u, marker_list_.size());

  EXPECT_EQ(0u, marker_list_[0]->StartOffset());
  EXPECT_EQ(10u, marker_list_[0]->EndOffset());

  EXPECT_EQ(40u, marker_list_[1]->StartOffset());
  EXPECT_EQ(50u, marker_list_[1]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_Empty) {
  DocumentMarker* marker =
      UnsortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
          marker_list_, 0, 10);
  EXPECT_EQ(nullptr, marker);
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_TouchingStart) {
  marker_list_.push_back(CreateMarker(1, 10));
  marker_list_.push_back(CreateMarker(0, 10));

  DocumentMarker* marker =
      UnsortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
          marker_list_, 0, 1);

  EXPECT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_TouchingEnd) {
  marker_list_.push_back(CreateMarker(0, 9));
  marker_list_.push_back(CreateMarker(0, 10));

  DocumentMarker* marker =
      UnsortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
          marker_list_, 9, 10);

  EXPECT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_TouchingStart) {
  marker_list_.push_back(CreateMarker(0, 9));
  marker_list_.push_back(CreateMarker(1, 9));
  marker_list_.push_back(CreateMarker(0, 10));
  marker_list_.push_back(CreateMarker(1, 10));

  UnsortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      UnsortedDocumentMarkerListEditor::MarkersIntersectingRange(marker_list_,
                                                                 0, 1);

  EXPECT_EQ(2u, markers_intersecting_range.size());

  EXPECT_EQ(0u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(9u, markers_intersecting_range[0]->EndOffset());

  EXPECT_EQ(0u, markers_intersecting_range[1]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[1]->EndOffset());
}

TEST_F(UnsortedDocumentMarkerListEditorTest,
       MarkersIntersectingRange_TouchingEnd) {
  marker_list_.push_back(CreateMarker(0, 9));
  marker_list_.push_back(CreateMarker(1, 9));
  marker_list_.push_back(CreateMarker(0, 10));
  marker_list_.push_back(CreateMarker(1, 10));

  UnsortedDocumentMarkerListEditor::MarkerList markers_intersecting_range =
      UnsortedDocumentMarkerListEditor::MarkersIntersectingRange(marker_list_,
                                                                 9, 10);

  EXPECT_EQ(2u, markers_intersecting_range.size());

  EXPECT_EQ(0u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());

  EXPECT_EQ(1u, markers_intersecting_range[1]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[1]->EndOffset());
}

}  // namespace blink
