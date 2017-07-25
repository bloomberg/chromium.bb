// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/DocumentMarkerListEditor.h"

#include "core/editing/markers/TextMatchMarker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class DocumentMarkerListEditorTest : public ::testing::Test {
 protected:
  DocumentMarker* CreateMarker(unsigned startOffset, unsigned endOffset) {
    return new TextMatchMarker(startOffset, endOffset,
                               TextMatchMarker::MatchStatus::kInactive);
  }
};

TEST_F(DocumentMarkerListEditorTest, RemoveMarkersEmptyList) {
  DocumentMarkerListEditor::MarkerList markers;
  DocumentMarkerListEditor::RemoveMarkers(&markers, 0, 10);
  EXPECT_EQ(0u, markers.size());
}

TEST_F(DocumentMarkerListEditorTest, RemoveMarkersTouchingEndpoints) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));
  markers.push_back(CreateMarker(10, 20));
  markers.push_back(CreateMarker(20, 30));

  DocumentMarkerListEditor::RemoveMarkers(&markers, 10, 10);

  EXPECT_EQ(2u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  EXPECT_EQ(20u, markers[1]->StartOffset());
  EXPECT_EQ(30u, markers[1]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest, RemoveMarkersOneCharacterIntoInterior) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));
  markers.push_back(CreateMarker(10, 20));
  markers.push_back(CreateMarker(20, 30));

  DocumentMarkerListEditor::RemoveMarkers(&markers, 9, 12);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceStartOfMarker) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  DocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 0, 5, 5);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceStartOfMarker) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  // Replace with shorter text
  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 5, 4);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(9u, markers[0]->EndOffset());

  // Replace with longer text
  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 4, 5);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  // Replace with text of same length
  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 5, 5);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceContainsStartOfMarker) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 15));

  DocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 0, 10, 10);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceContainsStartOfMarker) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 15));

  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 10, 10);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers[0]->StartOffset());
  EXPECT_EQ(15u, markers[0]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceEndOfMarker) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  DocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 5, 5, 5);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceEndOfMarker) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  // Replace with shorter text
  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5, 5, 4);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(9u, markers[0]->EndOffset());

  // Replace with longer text
  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5, 4, 5);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  // Replace with text of same length
  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5, 5, 5);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceContainsEndOfMarker) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  DocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 5, 10, 10);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceContainsEndOfMarker) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5, 10, 10);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceEntireMarker) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  DocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 0, 10, 10);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceEntireMarker) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  // Replace with shorter text
  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 10, 9);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(9u, markers[0]->EndOffset());

  // Replace with longer text
  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 9, 10);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  // Replace with text of same length
  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 10, 10);

  EXPECT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceTextWithMarkerAtBeginning) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  DocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 0, 15, 15);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceTextWithMarkerAtBeginning) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 15, 15);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentDependentMarker_ReplaceTextWithMarkerAtEnd) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 15));

  DocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 0, 15, 15);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentIndependentMarker_ReplaceTextWithMarkerAtEnd) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 15));

  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 15, 15);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(DocumentMarkerListEditorTest, ContentDependentMarker_Deletions) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));
  markers.push_back(CreateMarker(15, 20));
  markers.push_back(CreateMarker(20, 25));

  // Delete range containing the end of the second marker, the entire third
  // marker, and the start of the fourth marker
  DocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 8, 9, 0);

  EXPECT_EQ(2u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(11u, markers[1]->StartOffset());
  EXPECT_EQ(16u, markers[1]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest, ContentIndependentMarker_Deletions) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));
  markers.push_back(CreateMarker(15, 20));
  markers.push_back(CreateMarker(20, 25));

  // Delete range containing the end of the second marker, the entire third
  // marker, and the start of the fourth marker
  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 8, 9, 0);

  EXPECT_EQ(4u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(5u, markers[1]->StartOffset());
  EXPECT_EQ(8u, markers[1]->EndOffset());

  EXPECT_EQ(8u, markers[2]->StartOffset());
  EXPECT_EQ(11u, markers[2]->EndOffset());

  EXPECT_EQ(11u, markers[3]->StartOffset());
  EXPECT_EQ(16u, markers[3]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentDependentMarker_DeleteExactlyOnMarker) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  DocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 0, 10, 0);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentIndependentMarker_DeleteExactlyOnMarker) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 10));

  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 0, 10, 0);

  EXPECT_EQ(0u, markers.size());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentDependentMarker_InsertInMarkerInterior) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));

  // insert in middle of second marker
  DocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 7, 0, 5);

  EXPECT_EQ(2u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(15u, markers[1]->StartOffset());
  EXPECT_EQ(20u, markers[1]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentIndependentMarker_InsertInMarkerInterior) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));

  // insert in middle of second marker
  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 7, 0, 5);

  EXPECT_EQ(3u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(5u, markers[1]->StartOffset());
  EXPECT_EQ(15u, markers[1]->EndOffset());

  EXPECT_EQ(15u, markers[2]->StartOffset());
  EXPECT_EQ(20u, markers[2]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentDependentMarker_InsertBetweenMarkers) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));

  // insert before second marker
  DocumentMarkerListEditor::ShiftMarkersContentDependent(&markers, 5, 0, 5);

  EXPECT_EQ(3u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(10u, markers[1]->StartOffset());
  EXPECT_EQ(15u, markers[1]->EndOffset());

  EXPECT_EQ(15u, markers[2]->StartOffset());
  EXPECT_EQ(20u, markers[2]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest,
       ContentIndependentMarker_InsertBetweenMarkers) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));

  // insert before second marker
  DocumentMarkerListEditor::ShiftMarkersContentIndependent(&markers, 5, 0, 5);

  EXPECT_EQ(3u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  EXPECT_EQ(10u, markers[1]->StartOffset());
  EXPECT_EQ(15u, markers[1]->EndOffset());

  EXPECT_EQ(15u, markers[2]->StartOffset());
  EXPECT_EQ(20u, markers[2]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest, FirstMarkerIntersectingRange_Empty) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));

  DocumentMarker* marker =
      DocumentMarkerListEditor::FirstMarkerIntersectingRange(markers, 10, 15);
  EXPECT_EQ(nullptr, marker);
}

TEST_F(DocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_TouchingAfter) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));

  DocumentMarker* marker =
      DocumentMarkerListEditor::FirstMarkerIntersectingRange(markers, 5, 10);
  EXPECT_EQ(nullptr, marker);
}

TEST_F(DocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_TouchingBefore) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  DocumentMarker* marker =
      DocumentMarkerListEditor::FirstMarkerIntersectingRange(markers, 0, 5);
  EXPECT_EQ(nullptr, marker);
}

TEST_F(DocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_IntersectingAfter) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  DocumentMarker* marker =
      DocumentMarkerListEditor::FirstMarkerIntersectingRange(markers, 0, 6);
  EXPECT_NE(nullptr, marker);

  EXPECT_EQ(5u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_IntersectingBefore) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  DocumentMarker* marker =
      DocumentMarkerListEditor::FirstMarkerIntersectingRange(markers, 9, 15);
  EXPECT_NE(nullptr, marker);

  EXPECT_EQ(5u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest,
       FirstMarkerIntersectingRange_MultipleMarkers) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));
  markers.push_back(CreateMarker(15, 20));
  markers.push_back(CreateMarker(20, 25));

  DocumentMarker* marker =
      DocumentMarkerListEditor::FirstMarkerIntersectingRange(markers, 7, 17);
  EXPECT_NE(nullptr, marker);

  EXPECT_EQ(5u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest, MarkersIntersectingRange_Empty) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));

  DocumentMarkerListEditor::MarkerList markers_intersecting_range =
      DocumentMarkerListEditor::MarkersIntersectingRange(markers, 10, 15);
  EXPECT_EQ(0u, markers_intersecting_range.size());
}

TEST_F(DocumentMarkerListEditorTest, MarkersIntersectingRange_TouchingAfter) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));

  DocumentMarkerListEditor::MarkerList markers_intersecting_range =
      DocumentMarkerListEditor::MarkersIntersectingRange(markers, 5, 10);
  EXPECT_EQ(0u, markers_intersecting_range.size());
}

TEST_F(DocumentMarkerListEditorTest, MarkersIntersectingRange_TouchingBefore) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  DocumentMarkerListEditor::MarkerList markers_intersecting_range =
      DocumentMarkerListEditor::MarkersIntersectingRange(markers, 0, 5);
  EXPECT_EQ(0u, markers_intersecting_range.size());
}

TEST_F(DocumentMarkerListEditorTest,
       MarkersIntersectingRange_IntersectingAfter) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  DocumentMarkerListEditor::MarkerList markers_intersecting_range =
      DocumentMarkerListEditor::MarkersIntersectingRange(markers, 0, 6);
  EXPECT_EQ(1u, markers_intersecting_range.size());

  EXPECT_EQ(5u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest,
       MarkersIntersectingRange_IntersectingBefore) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  DocumentMarkerListEditor::MarkerList markers_intersecting_range =
      DocumentMarkerListEditor::MarkersIntersectingRange(markers, 9, 15);
  EXPECT_EQ(1u, markers_intersecting_range.size());

  EXPECT_EQ(5u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest, MarkersIntersectingRange_CollapsedRange) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(5, 10));

  DocumentMarkerListEditor::MarkerList markers_intersecting_range =
      DocumentMarkerListEditor::MarkersIntersectingRange(markers, 7, 7);
  EXPECT_EQ(1u, markers_intersecting_range.size());

  EXPECT_EQ(5u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());
}

TEST_F(DocumentMarkerListEditorTest, MarkersIntersectingRange_MultipleMarkers) {
  DocumentMarkerListEditor::MarkerList markers;
  markers.push_back(CreateMarker(0, 5));
  markers.push_back(CreateMarker(5, 10));
  markers.push_back(CreateMarker(10, 15));
  markers.push_back(CreateMarker(15, 20));
  markers.push_back(CreateMarker(20, 25));

  DocumentMarkerListEditor::MarkerList markers_intersecting_range =
      DocumentMarkerListEditor::MarkersIntersectingRange(markers, 7, 17);
  EXPECT_EQ(3u, markers_intersecting_range.size());

  EXPECT_EQ(5u, markers_intersecting_range[0]->StartOffset());
  EXPECT_EQ(10u, markers_intersecting_range[0]->EndOffset());

  EXPECT_EQ(10u, markers_intersecting_range[1]->StartOffset());
  EXPECT_EQ(15u, markers_intersecting_range[1]->EndOffset());

  EXPECT_EQ(15u, markers_intersecting_range[2]->StartOffset());
  EXPECT_EQ(20u, markers_intersecting_range[2]->EndOffset());
}

}  // namespace blink
