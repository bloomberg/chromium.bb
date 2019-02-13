// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor.h"

#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/scroll/scroll_alignment.h"

namespace blink {

namespace {

constexpr char kTextFragmentIdentifierPrefix[] = "targetText=";
constexpr size_t kTextFragmentIdentifierPrefixLength =
    base::size(kTextFragmentIdentifierPrefix);

bool ParseTargetTextIdentifier(const String& fragment,
                               String* start,
                               String* end) {
  if (fragment.Find(kTextFragmentIdentifierPrefix) != 0)
    return false;

  size_t comma_pos = fragment.find(',');
  size_t start_pos = kTextFragmentIdentifierPrefixLength - 1;

  if (comma_pos == kNotFound) {
    *start = fragment.Substring(start_pos);
    *end = "";
  } else {
    *start = fragment.Substring(start_pos, comma_pos - start_pos);

    size_t second_start_pos = comma_pos + 1;
    size_t end_pos = fragment.find('&', comma_pos);

    if (end_pos == kNotFound)
      *end = fragment.Substring(second_start_pos);
    else
      *end = fragment.Substring(second_start_pos, end_pos - second_start_pos);
  }

  return true;
}

}  // namespace

TextFragmentAnchor* TextFragmentAnchor::TryCreate(const KURL& url,
                                                  LocalFrame& frame) {
  String start;
  String end;

  if (!ParseTargetTextIdentifier(url.FragmentIdentifier(), &start, &end))
    return nullptr;

  String decoded_start = DecodeURLEscapeSequences(start, DecodeURLMode::kUTF8);
  String decoded_end = DecodeURLEscapeSequences(end, DecodeURLMode::kUTF8);

  return MakeGarbageCollected<TextFragmentAnchor>(decoded_start, decoded_end,
                                                  frame);
}

TextFragmentAnchor::TextFragmentAnchor(const String& start,
                                       const String& end,
                                       LocalFrame& frame)
    : start_(start), end_(end), finder_(*this), frame_(&frame) {
  DCHECK(frame_->View());
}

bool TextFragmentAnchor::Invoke() {
  if (search_finished_)
    return false;

  // TODO(bokan): We need to add the capability to match a snippet based on
  // it's start and end. https://crbug.com/924964.
  finder_.FindMatch(start_, *frame_->GetDocument());

  // Scrolling into view from the call above might cause us to set
  // search_finished_ so recompute here.
  search_finished_ = frame_->GetDocument()->IsLoadCompleted();

  return !search_finished_;
}

void TextFragmentAnchor::Installed() {}

void TextFragmentAnchor::DidScroll(ScrollType type) {
  if (!IsExplicitScrollType(type))
    return;

  search_finished_ = true;
}

void TextFragmentAnchor::PerformPreRafActions() {}

void TextFragmentAnchor::DidCompleteLoad() {
  // If there is a pending layout we'll finish the search from Invoke.
  if (!frame_->View()->NeedsLayout())
    search_finished_ = true;
}

void TextFragmentAnchor::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  FragmentAnchor::Trace(visitor);
}

void TextFragmentAnchor::DidFindMatch(const EphemeralRangeInFlatTree& range) {
  if (search_finished_)
    return;

  Document& document = *frame_->GetDocument();

  document.Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::TextMatch());

  LayoutRect bounding_box = LayoutRect(ComputeTextRect(range));

  DCHECK(range.Nodes().begin() != range.Nodes().end());

  Node& node = *range.Nodes().begin();

  DCHECK(node.GetLayoutObject());

  node.GetLayoutObject()->ScrollRectToVisible(
      bounding_box,
      WebScrollIntoViewParams(ScrollAlignment::kAlignCenterIfNeeded,
                              ScrollAlignment::kAlignCenterIfNeeded,
                              kProgrammaticScroll));

  EphemeralRange dom_range =
      EphemeralRange(ToPositionInDOMTree(range.StartPosition()),
                     ToPositionInDOMTree(range.EndPosition()));
  document.Markers().AddTextMatchMarker(dom_range,
                                        TextMatchMarker::MatchStatus::kActive);
  frame_->GetEditor().SetMarkedTextMatchesAreHighlighted(true);
}

}  // namespace blink
