// Copyright 2017 The Chromium Authors. All rights reserved.  Use of
// this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/SuggestionMarkerListImpl.h"

#include "core/editing/markers/SuggestionMarker.h"
#include "core/editing/markers/SuggestionMarkerReplacementScope.h"
#include "core/editing/markers/UnsortedDocumentMarkerListEditor.h"
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

// TODO(rlanday): split UnsortedDocumentMarkerListEditorTest into its own file.
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

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForSuggestionReplacement_ReturnsFalseWhenNoShift) {
  marker_list_->Add(CreateMarker(0, 10));

  {
    SuggestionMarkerReplacementScope scope;
    // Replace range 0 to 10 with a ten character string.
    // Text is ignored for suggestion replacement, so we can just pass an empty
    // string.
    EXPECT_FALSE(marker_list_->ShiftMarkers("", 0, 10, 10));
  }

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(1u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForSuggestionReplacement_MarkersUpdateProperly) {
  // Marker with suggestion to apply.
  // Should be kept (and shifted).
  marker_list_->Add(CreateMarker(10, 20));

  // Marker touching start of replacement range.
  // Should be kept.
  marker_list_->Add(CreateMarker(0, 10));

  // Marker partially overlapping start of replacement range.
  // Should be removed,
  marker_list_->Add(CreateMarker(0, 11));

  // Marker touching end of replacement range.
  // Should be kept (and shifted).
  marker_list_->Add(CreateMarker(20, 30));

  // Marker partially overlapping end of replacement range
  // Should be removed.
  marker_list_->Add(CreateMarker(19, 30));

  // Marker contained inside replacement range
  // Should be removed.
  marker_list_->Add(CreateMarker(11, 19));

  // Marker containing replacement range
  // Should be kept (and shifted).
  marker_list_->Add(CreateMarker(9, 21));

  {
    SuggestionMarkerReplacementScope scope;
    // Replace range 10 to 20 with a five character string.
    // Text is ignored for suggestion replacement, so we can just pass an empty
    // string.
    EXPECT_TRUE(marker_list_->ShiftMarkers("", 10, 10, 5));
  }

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  std::sort(markers.begin(), markers.end(), compare_markers);

  EXPECT_EQ(4u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(10u, markers[0]->EndOffset());

  EXPECT_EQ(9u, markers[1]->StartOffset());
  EXPECT_EQ(16u, markers[1]->EndOffset());

  EXPECT_EQ(10u, markers[2]->StartOffset());
  EXPECT_EQ(15u, markers[2]->EndOffset());

  EXPECT_EQ(15u, markers[3]->StartOffset());
  EXPECT_EQ(25u, markers[3]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_DeleteFromMiddle) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers("hello", 2, 1, 0));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_InsertIntoMiddle) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers("hello", 2, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_PrependLetter) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers("ahello", 0, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(
    SuggestionMarkerListImplTest,
    ShiftMarkersForNonSuggestionEditingOperation_PrependSurrogatePairLetter) {
  marker_list_->Add(CreateMarker(0, 5));

  // Prepending MATHEMATICAL SCRIPT CAPITAL C
  EXPECT_TRUE(marker_list_->ShiftMarkers(u"\U0001d49ehello", 0, 0, 2));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_PrependDigit) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers("0hello", 0, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_PrependSurrogatePairDigit) {
  marker_list_->Add(CreateMarker(0, 5));

  // Prepending MATHEMATICAL DOUBLE-STRUCK DIGIT ONE
  EXPECT_TRUE(marker_list_->ShiftMarkers(u"\U0001d7d9hello", 0, 0, 2));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_PrependNonAlphanumeric) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers(".hello", 0, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();

  EXPECT_EQ(1u, markers.size());

  EXPECT_EQ(1u, markers[0]->StartOffset());
  EXPECT_EQ(6u, markers[0]->EndOffset());
}

TEST_F(
    SuggestionMarkerListImplTest,
    ShiftMarkersForNonSuggestionEditingOperation_PrependSurrogatePairNonAlphanumeric) {
  marker_list_->Add(CreateMarker(0, 5));

  // Prepending FACE WITH TEARS OF JOY
  EXPECT_TRUE(marker_list_->ShiftMarkers(u"\U0001f602hello", 0, 0, 2));

  DocumentMarkerVector markers = marker_list_->GetMarkers();

  EXPECT_EQ(1u, markers.size());

  EXPECT_EQ(2u, markers[0]->StartOffset());
  EXPECT_EQ(7u, markers[0]->EndOffset());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_AppendLetter) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers("helloa", 5, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_AppendSurrogatePairLetter) {
  marker_list_->Add(CreateMarker(0, 5));

  // Appending MATHEMATICAL SCRIPT CAPITAL C
  EXPECT_TRUE(marker_list_->ShiftMarkers(u"hello\U0001d49e", 5, 0, 2));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_AppendDigit) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_TRUE(marker_list_->ShiftMarkers("hello0", 5, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_AppendSurrogatePairDigit) {
  marker_list_->Add(CreateMarker(0, 5));

  // Appending MATHEMATICAL DOUBLE-STRUCK DIGIT ONE
  EXPECT_TRUE(marker_list_->ShiftMarkers(u"hello\U0001d7d9", 5, 0, 2));

  DocumentMarkerVector markers = marker_list_->GetMarkers();
  EXPECT_EQ(0u, markers.size());
}

TEST_F(SuggestionMarkerListImplTest,
       ShiftMarkersForNonSuggestionEditingOperation_AppendNonAlphanumeric) {
  marker_list_->Add(CreateMarker(0, 5));

  EXPECT_FALSE(marker_list_->ShiftMarkers("hello.", 5, 0, 1));

  DocumentMarkerVector markers = marker_list_->GetMarkers();

  EXPECT_EQ(1u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());
}

TEST_F(
    SuggestionMarkerListImplTest,
    ShiftMarkersForNonSuggestionEditingOperation_AppendSurrogatePairNonAlphanumeric) {
  marker_list_->Add(CreateMarker(0, 5));

  // Appending FACE WITH TEARS OF JOY
  EXPECT_FALSE(marker_list_->ShiftMarkers(u"hello\U0001f602", 5, 0, 2));

  DocumentMarkerVector markers = marker_list_->GetMarkers();

  EXPECT_EQ(1u, markers.size());

  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());
}

}  // namespace blink
