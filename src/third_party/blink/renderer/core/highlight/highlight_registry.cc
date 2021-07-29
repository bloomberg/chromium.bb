// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight_registry.h"

#include "third_party/blink/renderer/core/dom/abstract_range.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

HighlightRegistry* HighlightRegistry::From(LocalDOMWindow& window) {
  HighlightRegistry* supplement =
      Supplement<LocalDOMWindow>::From<HighlightRegistry>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<HighlightRegistry>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, supplement);
  }
  return supplement;
}

HighlightRegistry::HighlightRegistry(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window), frame_(window.GetFrame()) {}

HighlightRegistry::~HighlightRegistry() = default;

const char HighlightRegistry::kSupplementName[] = "HighlightRegistry";

void HighlightRegistry::Trace(blink::Visitor* visitor) const {
  visitor->Trace(highlights_);
  visitor->Trace(frame_);
  ScriptWrappable::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// Deletes all HighlightMarkers and rebuilds them with the contents of
// highlights_.
void HighlightRegistry::ValidateHighlightMarkers() {
  Document* document = frame_->GetDocument();
  if (!document)
    return;

  document->Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::Highlight());

  for (const auto& highlight_registry_map_entry : highlights_) {
    const auto& highlight_name = highlight_registry_map_entry->highlight_name;
    const auto& highlight = highlight_registry_map_entry->highlight;
    for (const auto& abstract_range : highlight->GetRanges()) {
      if (!abstract_range->collapsed()) {
        auto* static_range = DynamicTo<StaticRange>(*abstract_range);
        if (static_range && (!static_range->IsValid() ||
                             static_range->CrossesContainBoundary()))
          continue;

        EphemeralRange eph_range(abstract_range);
        document->Markers().AddHighlightMarker(eph_range, highlight_name,
                                               highlight);
      }
    }
  }
}

void HighlightRegistry::ScheduleRepaint() const {
  if (LocalFrameView* local_frame_view = frame_->View()) {
    local_frame_view->ScheduleVisualUpdateForPaintInvalidationIfNeeded();
  }
}

HighlightRegistry* HighlightRegistry::setForBinding(
    ScriptState* script_state,
    AtomicString highlight_name,
    Member<Highlight> highlight,
    ExceptionState& exception_state) {
  auto highlights_iterator = GetMapIterator(highlight_name);
  if (highlights_iterator != highlights_.end()) {
    highlights_iterator->Get()->highlight->Deregister();
    // It's necessary to delete it and insert a new entry to the registry
    // instead of just modifying the existing one so the insertion order is
    // preserved.
    highlights_.erase(highlights_iterator);
  }
  highlights_.insert(MakeGarbageCollected<HighlightRegistryMapEntry>(
      highlight_name, highlight));
  highlight->RegisterIn(this);
  ScheduleRepaint();
  return this;
}

void HighlightRegistry::clearForBinding(ScriptState*, ExceptionState&) {
  for (const auto& highlight_registry_map_entry : highlights_) {
    highlight_registry_map_entry->highlight->Deregister();
  }
  highlights_.clear();
  ScheduleRepaint();
}

bool HighlightRegistry::deleteForBinding(ScriptState*,
                                         const AtomicString& highlight_name,
                                         ExceptionState&) {
  auto highlights_iterator = GetMapIterator(highlight_name);
  if (highlights_iterator != highlights_.end()) {
    highlights_iterator->Get()->highlight->Deregister();
    highlights_.erase(highlights_iterator);
    ScheduleRepaint();
    return true;
  }

  return false;
}

int8_t HighlightRegistry::CompareOverlayStackingPosition(
    const AtomicString& highlight_name1,
    const Highlight* highlight1,
    const AtomicString& highlight_name2,
    const Highlight* highlight2) const {
  if (highlight_name1 == highlight_name2)
    return kOverlayStackingPositionEquivalent;

  if (highlight1->priority() == highlight2->priority()) {
    for (const auto& highlight_registry_map_entry : highlights_) {
      const auto& highlight_name = highlight_registry_map_entry->highlight_name;
      if (highlight_name == highlight_name1) {
        DCHECK(highlight1 == highlight_registry_map_entry->highlight);
        return kOverlayStackingPositionBelow;
      }
      if (highlight_name == highlight_name2) {
        DCHECK(highlight2 == highlight_registry_map_entry->highlight);
        return kOverlayStackingPositionAbove;
      }
    }
    NOTREACHED();
    return kOverlayStackingPositionEquivalent;
  }

  return highlight1->priority() > highlight2->priority()
             ? kOverlayStackingPositionAbove
             : kOverlayStackingPositionBelow;
}

HighlightRegistry::IterationSource::IterationSource(
    const HighlightRegistry& highlight_registry)
    : index_(0) {
  highlights_snapshot_.ReserveInitialCapacity(
      highlight_registry.highlights_.size());
  for (const auto& highlight_registry_map_entry :
       highlight_registry.highlights_) {
    highlights_snapshot_.push_back(
        MakeGarbageCollected<HighlightRegistryMapEntry>(
            highlight_registry_map_entry));
  }
}

bool HighlightRegistry::IterationSource::Next(ScriptState*,
                                              AtomicString& key,
                                              Member<Highlight>& value,
                                              ExceptionState&) {
  if (index_ >= highlights_snapshot_.size())
    return false;
  key = highlights_snapshot_[index_]->highlight_name;
  value = highlights_snapshot_[index_++]->highlight;
  return true;
}

void HighlightRegistry::IterationSource::Trace(blink::Visitor* visitor) const {
  visitor->Trace(highlights_snapshot_);
  HighlightRegistryMapIterable::IterationSource::Trace(visitor);
}

HighlightRegistryMapIterable::IterationSource*
HighlightRegistry::StartIteration(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<IterationSource>(*this);
}

}  // namespace blink
