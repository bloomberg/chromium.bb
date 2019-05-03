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
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"

namespace blink {

namespace {

constexpr char kTextFragmentIdentifierPrefix[] = "targetText=";
constexpr size_t kTextFragmentIdentifierPrefixArrayLength =
    base::size(kTextFragmentIdentifierPrefix);

bool ParseTargetTextIdentifier(
    const String& fragment,
    std::vector<TextFragmentSelector>* out_selectors) {
  DCHECK(out_selectors);

  size_t start_pos = 0;
  size_t end_pos = 0;
  while (end_pos != kNotFound) {
    if (fragment.Find(kTextFragmentIdentifierPrefix, start_pos) != start_pos)
      return false;

    // The prefix array length includes the \0 string terminator.
    start_pos += kTextFragmentIdentifierPrefixArrayLength - 1;
    end_pos = fragment.find('&', start_pos);

    String target_text;
    if (end_pos == kNotFound) {
      target_text = fragment.Substring(start_pos);
    } else {
      target_text = fragment.Substring(start_pos, end_pos - start_pos);
      start_pos = end_pos + 1;
    }
    out_selectors->push_back(TextFragmentSelector::Create(target_text));
  }

  return true;
}

}  // namespace

TextFragmentAnchor* TextFragmentAnchor::TryCreate(const KURL& url,
                                                  LocalFrame& frame) {
  std::vector<TextFragmentSelector> selectors;

  if (!ParseTargetTextIdentifier(url.FragmentIdentifier(), &selectors))
    return nullptr;

  return MakeGarbageCollected<TextFragmentAnchor>(selectors, frame);
}

TextFragmentAnchor::TextFragmentAnchor(
    const std::vector<TextFragmentSelector>& text_fragment_selectors,
    LocalFrame& frame)
    : frame_(&frame) {
  DCHECK(!text_fragment_selectors.empty());
  DCHECK(frame_->View());

  text_fragment_finders_.reserve(text_fragment_selectors.size());
  for (TextFragmentSelector selector : text_fragment_selectors)
    text_fragment_finders_.emplace_back(*this, selector);
}

bool TextFragmentAnchor::Invoke() {
  if (search_finished_)
    return false;

  frame_->GetDocument()->Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::TextMatch());

  if (!user_scrolled_)
    first_match_needs_scroll_ = true;

  for (auto& finder : text_fragment_finders_)
    finder.FindMatch(*frame_->GetDocument());

  // Scrolling into view from the call above might cause us to set
  // search_finished_ so recompute here.
  search_finished_ = frame_->GetDocument()->IsLoadCompleted();

  return !search_finished_;
}

void TextFragmentAnchor::Installed() {}

void TextFragmentAnchor::DidScroll(ScrollType type) {
  if (!IsExplicitScrollType(type))
    return;

  user_scrolled_ = true;
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

  if (first_match_needs_scroll_) {
    first_match_needs_scroll_ = false;

    LayoutRect bounding_box = LayoutRect(ComputeTextRect(range));

    // Set the bounding box height to zero because we want to center the top of
    // the text range.
    bounding_box.SetHeight(LayoutUnit());

    DCHECK(range.Nodes().begin() != range.Nodes().end());

    Node& node = *range.Nodes().begin();

    DCHECK(node.GetLayoutObject());

    node.GetLayoutObject()->ScrollRectToVisible(
        bounding_box,
        WebScrollIntoViewParams(ScrollAlignment::kAlignCenterIfNeeded,
                                ScrollAlignment::kAlignCenterIfNeeded,
                                kProgrammaticScroll));
  }
  EphemeralRange dom_range =
      EphemeralRange(ToPositionInDOMTree(range.StartPosition()),
                     ToPositionInDOMTree(range.EndPosition()));
  // TODO(nburris): Determine what we should do with overlapping text matches.
  // Currently, AddTextMatchMarker will crash when adding an overlapping marker.
  frame_->GetDocument()->Markers().AddTextMatchMarker(
      dom_range, TextMatchMarker::MatchStatus::kInactive);
  frame_->GetEditor().SetMarkedTextMatchesAreHighlighted(true);
}

}  // namespace blink
