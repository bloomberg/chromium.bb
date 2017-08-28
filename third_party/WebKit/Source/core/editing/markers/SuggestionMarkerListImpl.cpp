// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/SuggestionMarkerListImpl.h"

#include "core/editing/markers/SortedDocumentMarkerListEditor.h"
#include "core/editing/markers/SuggestionMarker.h"
#include "core/editing/markers/SuggestionMarkerReplacementScope.h"

namespace blink {

namespace {

UChar32 GetCodePointAt(const String& text, size_t index) {
  UChar32 c;
  U16_GET(text, 0, index, text.length(), c);
  return c;
}

Optional<DocumentMarker::MarkerOffsets>
ComputeOffsetsAfterNonSuggestionEditingOperating(const DocumentMarker& marker,
                                                 const String& node_text,
                                                 unsigned offset,
                                                 unsigned old_length,
                                                 unsigned new_length) {
  unsigned marker_start = marker.StartOffset();
  unsigned marker_end = marker.EndOffset();

  // Marked text was modified
  if (offset < marker_end && offset + old_length > marker_start)
    return {};

  // Text inserted/replaced immediately after the marker, remove marker if first
  // character is a (Unicode) letter or digit
  if (offset == marker_end && new_length > 0) {
    if (WTF::Unicode::IsAlphanumeric(GetCodePointAt(node_text, offset)))
      return {};
    return marker.ComputeOffsetsAfterShift(offset, old_length, new_length);
  }

  // Text inserted/replaced immediately before the marker, remove marker if
  // first character is a (Unicode) letter or digit
  if (offset == marker_start && new_length > 0) {
    if (WTF::Unicode::IsAlphanumeric(
            GetCodePointAt(node_text, offset + new_length - 1)))
      return {};
    return marker.ComputeOffsetsAfterShift(offset, old_length, new_length);
  }

  // Don't care if text was deleted immediately before or after the marker
  return marker.ComputeOffsetsAfterShift(offset, old_length, new_length);
}

}  // namespace

DocumentMarker::MarkerType SuggestionMarkerListImpl::MarkerType() const {
  return DocumentMarker::kSuggestion;
}

bool SuggestionMarkerListImpl::IsEmpty() const {
  return markers_.IsEmpty();
}

void SuggestionMarkerListImpl::Add(DocumentMarker* marker) {
  markers_.push_back(marker);
}

void SuggestionMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>& SuggestionMarkerListImpl::GetMarkers()
    const {
  return markers_;
}

DocumentMarker* SuggestionMarkerListImpl::FirstMarkerIntersectingRange(
    unsigned start_offset,
    unsigned end_offset) const {
  DCHECK_LE(start_offset, end_offset);

  const auto it =
      std::find_if(markers_.begin(), markers_.end(),
                   [start_offset, end_offset](const DocumentMarker* marker) {
                     return marker->StartOffset() < end_offset &&
                            marker->EndOffset() > start_offset;
                   });

  if (it == markers_.end())
    return nullptr;
  return *it;
}

HeapVector<Member<DocumentMarker>>
SuggestionMarkerListImpl::MarkersIntersectingRange(unsigned start_offset,
                                                   unsigned end_offset) const {
  DCHECK_LE(start_offset, end_offset);

  HeapVector<Member<DocumentMarker>> results;
  std::copy_if(markers_.begin(), markers_.end(), std::back_inserter(results),
               [start_offset, end_offset](const DocumentMarker* marker) {
                 return marker->StartOffset() < end_offset &&
                        marker->EndOffset() > start_offset;
               });
  return results;
}

bool SuggestionMarkerListImpl::MoveMarkers(int length,
                                           DocumentMarkerList* dst_list) {
  DCHECK_GT(length, 0);
  bool did_move_marker = false;
  unsigned end_offset = length - 1;

  HeapVector<Member<DocumentMarker>> unmoved_markers;
  for (auto it = markers_.begin(); it != markers_.end(); ++it) {
    DocumentMarker& marker = **it;
    if (marker.StartOffset() > end_offset) {
      unmoved_markers.push_back(marker);
      continue;
    }

    // If we're splitting a text node in the middle of a suggestion marker,
    // remove the marker
    if (marker.EndOffset() > end_offset)
      continue;

    dst_list->Add(&marker);
    did_move_marker = true;
  }

  markers_ = std::move(unmoved_markers);
  return did_move_marker;
}

bool SuggestionMarkerListImpl::RemoveMarkers(unsigned start_offset,
                                             int length) {
  // Since suggestion markers are stored unsorted, the quickest way to perform
  // this operation is to build a new list with the markers that aren't being
  // removed.
  const unsigned end_offset = start_offset + length;
  HeapVector<Member<DocumentMarker>> unremoved_markers;
  for (const Member<DocumentMarker>& marker : markers_) {
    if (marker->EndOffset() <= start_offset ||
        marker->StartOffset() >= end_offset) {
      unremoved_markers.push_back(marker);
      continue;
    }
  }

  const bool did_remove_marker = (unremoved_markers.size() != markers_.size());
  markers_ = std::move(unremoved_markers);
  return did_remove_marker;
}

bool SuggestionMarkerListImpl::ShiftMarkers(const String& node_text,
                                            unsigned offset,
                                            unsigned old_length,
                                            unsigned new_length) {
  if (SuggestionMarkerReplacementScope::CurrentlyInScope())
    return ShiftMarkersForSuggestionReplacement(offset, old_length, new_length);

  return ShiftMarkersForNonSuggestionEditingOperation(node_text, offset,
                                                      old_length, new_length);
}

bool SuggestionMarkerListImpl::ShiftMarkersForSuggestionReplacement(
    unsigned offset,
    unsigned old_length,
    unsigned new_length) {
  // Since suggestion markers are stored unsorted, the quickest way to perform
  // this operation is to build a new list with the markers not removed by the
  // shift.
  bool did_shift_marker = false;
  unsigned end_offset = offset + old_length;
  HeapVector<Member<DocumentMarker>> unremoved_markers;
  for (const Member<DocumentMarker>& marker : markers_) {
    // Markers that intersect the replacement range, but do not fully contain
    // it, should be removed.
    const bool marker_intersects_replacement_range =
        marker->StartOffset() < end_offset && marker->EndOffset() > offset;
    const bool marker_contains_replacement_range =
        marker->StartOffset() <= offset && marker->EndOffset() >= end_offset;

    if (marker_intersects_replacement_range &&
        !marker_contains_replacement_range) {
      did_shift_marker = true;
      continue;
    }

    Optional<DocumentMarker::MarkerOffsets> result =
        marker->ComputeOffsetsAfterShift(offset, old_length, new_length);
    if (result == WTF::nullopt) {
      did_shift_marker = true;
      continue;
    }

    if (marker->StartOffset() != result.value().start_offset ||
        marker->EndOffset() != result.value().end_offset) {
      marker->SetStartOffset(result.value().start_offset);
      marker->SetEndOffset(result.value().end_offset);
      did_shift_marker = true;
    }

    unremoved_markers.push_back(marker);
  }

  markers_ = std::move(unremoved_markers);
  return did_shift_marker;
}

bool SuggestionMarkerListImpl::ShiftMarkersForNonSuggestionEditingOperation(
    const String& node_text,
    unsigned offset,
    unsigned old_length,
    unsigned new_length) {
  // Since suggestion markers are stored unsorted, the quickest way to perform
  // this operation is to build a new list with the markers not removed by the
  // shift.
  bool did_shift_marker = false;
  HeapVector<Member<DocumentMarker>> unremoved_markers;
  for (const Member<DocumentMarker>& marker : markers_) {
    Optional<DocumentMarker::MarkerOffsets> result =
        ComputeOffsetsAfterNonSuggestionEditingOperating(
            *marker, node_text, offset, old_length, new_length);
    if (!result) {
      did_shift_marker = true;
      continue;
    }

    if (marker->StartOffset() != result.value().start_offset ||
        marker->EndOffset() != result.value().end_offset) {
      marker->SetStartOffset(result.value().start_offset);
      marker->SetEndOffset(result.value().end_offset);
      did_shift_marker = true;
    }

    unremoved_markers.push_back(marker);
  }

  markers_ = std::move(unremoved_markers);
  return did_shift_marker;
}

DEFINE_TRACE(SuggestionMarkerListImpl) {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

}  // namespace blink
