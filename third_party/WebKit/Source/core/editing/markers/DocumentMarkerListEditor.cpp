// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/DocumentMarkerListEditor.h"

#include "core/editing/markers/RenderedDocumentMarker.h"

namespace blink {

// TODO(rlanday): this method was created by cutting and pasting code from
// DocumentMarkerController::AddMarker(), it should be refactored in a future CL
void DocumentMarkerListEditor::AddMarker(MarkerList* list,
                                         const DocumentMarker* marker) {
  RenderedDocumentMarker* rendered_marker =
      RenderedDocumentMarker::Create(*marker);
  if (list->IsEmpty() || list->back()->EndOffset() < marker->StartOffset()) {
    list->push_back(rendered_marker);
  } else {
    if (marker->GetType() != DocumentMarker::kTextMatch &&
        marker->GetType() != DocumentMarker::kComposition) {
      MergeOverlapping(list, rendered_marker);
    } else {
      MarkerList::iterator pos = std::lower_bound(
          list->begin(), list->end(), marker,
          [](const Member<RenderedDocumentMarker>& marker_in_list,
             const DocumentMarker* marker_to_insert) {
            return marker_in_list->StartOffset() <
                   marker_to_insert->StartOffset();
          });
      list->insert(pos - list->begin(), rendered_marker);
    }
  }
}

bool DocumentMarkerListEditor::MoveMarkers(MarkerList* src_list,
                                           int length,
                                           MarkerList* dst_list) {
  DCHECK_GT(length, 0);
  bool didMoveMarker = false;
  unsigned end_offset = length - 1;

  MarkerList::iterator it;
  for (it = src_list->begin(); it != src_list->end(); ++it) {
    DocumentMarker& marker = **it;
    if (marker.StartOffset() > end_offset)
      break;

    // pin the marker to the specified range and apply the shift delta
    if (marker.EndOffset() > end_offset)
      marker.SetEndOffset(end_offset);

    DocumentMarkerListEditor::AddMarker(dst_list, &marker);
    didMoveMarker = true;
  }

  // Remove the range of markers that were moved to dstNode
  src_list->erase(0, it - src_list->begin());

  return didMoveMarker;
}

// TODO(rlanday): this method was created by cutting and pasting code from
// DocumentMarkerController::RemoveMarkers(), it should be refactored in a
// future CL
bool DocumentMarkerListEditor::RemoveMarkers(MarkerList* list,
                                             unsigned start_offset,
                                             int length) {
  bool doc_dirty = false;
  const unsigned end_offset = start_offset + length;
  MarkerList::iterator start_pos = std::upper_bound(
      list->begin(), list->end(), start_offset,
      [](size_t start_offset, const Member<RenderedDocumentMarker>& marker) {
        return start_offset < marker->EndOffset();
      });
  for (MarkerList::iterator i = start_pos; i != list->end();) {
    DocumentMarker marker(*i->Get());

    // markers are returned in order, so stop if we are now past the specified
    // range
    if (marker.StartOffset() >= end_offset)
      break;

    list->erase(i - list->begin());
    doc_dirty = true;
  }

  return doc_dirty;
}

bool DocumentMarkerListEditor::ShiftMarkers(MarkerList* list,
                                            unsigned offset,
                                            unsigned old_length,
                                            unsigned new_length) {
  bool did_shift_marker = false;
  for (MarkerList::iterator it = list->begin(); it != list->end(); ++it) {
    RenderedDocumentMarker& marker = **it;
    Optional<DocumentMarker::MarkerOffsets> result =
        marker.ComputeOffsetsAfterShift(offset, old_length, new_length);
    if (result == WTF::kNullopt) {
      list->erase(it - list->begin());
      --it;
      did_shift_marker = true;
      continue;
    }

    if (marker.StartOffset() != result.value().start_offset ||
        marker.EndOffset() != result.value().end_offset) {
      did_shift_marker = true;
      marker.SetStartOffset(result.value().start_offset);
      marker.SetEndOffset(result.value().end_offset);
    }
  }

  return did_shift_marker;
}

void DocumentMarkerListEditor::MergeOverlapping(
    DocumentMarkerListEditor::MarkerList* list,
    RenderedDocumentMarker* to_insert) {
  auto first_overlapping = std::lower_bound(
      list->begin(), list->end(), to_insert,
      [](const Member<RenderedDocumentMarker>& marker_in_list,
         const DocumentMarker* marker_to_insert) {
        return marker_in_list->EndOffset() < marker_to_insert->StartOffset();
      });
  size_t index = first_overlapping - list->begin();
  list->insert(index, to_insert);
  const auto inserted = list->begin() + index;
  first_overlapping = inserted + 1;
  for (const auto i = first_overlapping;
       i != list->end() && (*i)->StartOffset() <= (*inserted)->EndOffset();) {
    (*inserted)->SetStartOffset(
        std::min((*inserted)->StartOffset(), (*i)->StartOffset()));
    (*inserted)->SetEndOffset(
        std::max((*inserted)->EndOffset(), (*i)->EndOffset()));
    list->erase(i - list->begin());
  }
}

}  // namespace blink
