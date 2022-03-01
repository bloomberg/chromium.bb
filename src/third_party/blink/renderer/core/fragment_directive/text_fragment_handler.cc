// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/shared_highlighting/core/common/disabled_sites.h"
#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/range_in_flat_tree.h"
#include "third_party/blink/renderer/core/editing/selection_editor.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/fragment_directive/fragment_directive_utils.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"

namespace blink {

TextFragmentHandler::TextFragmentHandler(LocalFrame* main_frame)
    : text_fragment_selector_generator_(
          MakeGarbageCollected<TextFragmentSelectorGenerator>(main_frame)) {}

// TODO(http://crbug/1262142): lazily bind once and not re-bind each time.
void TextFragmentHandler::BindTextFragmentReceiver(
    mojo::PendingReceiver<mojom::blink::TextFragmentReceiver> producer) {
  selector_producer_.reset();
  selector_producer_.Bind(
      std::move(producer),
      text_fragment_selector_generator_->GetFrame()->GetTaskRunner(
          blink::TaskType::kInternalDefault));
}

void TextFragmentHandler::Cancel() {
  GetTextFragmentSelectorGenerator()->Reset();
}

void TextFragmentHandler::RequestSelector(RequestSelectorCallback callback) {
  DCHECK(shared_highlighting::ShouldOfferLinkToText(
      GetFrame()->GetDocument()->Url()));
  DCHECK(!GetFrame()->Selection().SelectedText().IsEmpty());

  GetTextFragmentSelectorGenerator()->RecordSelectorStateUma();

  selector_requested_before_ready_ = !preemptive_generation_result_.has_value();
  response_callback_ = std::move(callback);

  // If preemptive link generation is enabled, the generator would have
  // already been invoked when the selection was updated in
  // StartPreemptiveGenerationIfNeeded. If that generation finished simply
  // respond with the result. Otherwise, the response callback is stored so
  // that we reply on completion.
  if (!selector_requested_before_ready_.value())
    InvokeReplyCallback(preemptive_generation_result_.value(), error_);
}

void TextFragmentHandler::GetExistingSelectors(
    GetExistingSelectorsCallback callback) {
  Vector<String> text_fragment_selectors;

  TextFragmentAnchor* anchor = GetTextFragmentAnchor();
  if (!anchor) {
    std::move(callback).Run(Vector<String>());
    return;
  }

  for (auto& directive_finder_pair : anchor->DirectiveFinderPairs()) {
    TextFragmentFinder* finder = directive_finder_pair.second.Get();
    if (finder->FirstMatch()) {
      text_fragment_selectors.push_back(finder->GetSelector().ToString());
    }
  }

  std::move(callback).Run(text_fragment_selectors);
}

// TODO(http://crbug/1262141): look into using PageBroadcast Mojo.
void TextFragmentHandler::RemoveFragments() {
  DCHECK(
      base::FeatureList::IsEnabled(shared_highlighting::kSharedHighlightingV2));

  LocalFrame* frame = GetTextFragmentSelectorGenerator()->GetFrame();

  if (GetTextFragmentAnchor()) {
    frame->View()->DismissFragmentAnchor();
  } else if (frame->IsMainFrame()) {
    // DismissFragmentAnchor normally runs the URL update steps to remove the
    // selectors from the URL. However, even if the main frame doesn't have a
    // text fragment anchor, the selectors still need to be removed from the
    // URL. This is because dismissing the text fragment anchors is a page-wide
    // operation, and the URL might have selectors for a subframe.
    FragmentDirectiveUtils::RemoveSelectorsFromUrl(frame);
  }
}

// static
bool TextFragmentHandler::IsOverTextFragment(HitTestResult result) {
  if (!result.InnerNode() || !result.InnerNodeFrame()) {
    return false;
  }

  // Tree should be clean before accessing the position.
  // |HitTestResult::GetPosition| calls |PositionForPoint()| which requires
  // |kPrePaintClean|.
  DCHECK_GE(result.InnerNodeFrame()->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  DocumentMarkerController& marker_controller =
      result.InnerNodeFrame()->GetDocument()->Markers();
  PositionWithAffinity pos_with_affinity = result.GetPosition();
  const Position marker_position = pos_with_affinity.GetPosition();
  auto markers = marker_controller.MarkersAroundPosition(
      ToPositionInFlatTree(marker_position),
      DocumentMarker::MarkerTypes::TextFragment());
  return !markers.IsEmpty();
}

void TextFragmentHandler::ExtractTextFragmentsMatches(
    ExtractTextFragmentsMatchesCallback callback) {
  DCHECK(
      base::FeatureList::IsEnabled(shared_highlighting::kSharedHighlightingV2));
  Vector<String> text_fragment_matches;

  TextFragmentAnchor* anchor = GetTextFragmentAnchor();
  if (!anchor) {
    std::move(callback).Run(Vector<String>());
    return;
  }

  for (auto& directive_finder_pair : anchor->DirectiveFinderPairs()) {
    TextFragmentFinder* finder = directive_finder_pair.second.Get();
    if (finder->FirstMatch()) {
      text_fragment_matches.push_back(
          PlainText(finder->FirstMatch()->ToEphemeralRange()));
    }
  }

  std::move(callback).Run(text_fragment_matches);
}

void TextFragmentHandler::ExtractFirstFragmentRect(
    ExtractFirstFragmentRectCallback callback) {
  DCHECK(
      base::FeatureList::IsEnabled(shared_highlighting::kSharedHighlightingV2));
  gfx::Rect rect_in_viewport;

  TextFragmentAnchor* anchor = GetTextFragmentAnchor();
  if (!anchor || anchor->DirectiveFinderPairs().size() <= 0) {
    std::move(callback).Run(gfx::Rect());
    return;
  }

  for (auto& directive_finder_pair : anchor->DirectiveFinderPairs()) {
    TextFragmentFinder* finder = directive_finder_pair.second.Get();
    if (finder->FirstMatch() == nullptr) {
      continue;
    }

    PhysicalRect bounding_box(
        ComputeTextRect(finder->FirstMatch()->ToEphemeralRange()));
    rect_in_viewport =
        GetFrame()->View()->FrameToViewport(ToEnclosingRect(bounding_box));
    break;
  }

  std::move(callback).Run(rect_in_viewport);
}

void TextFragmentHandler::DidFinishSelectorGeneration(
    const TextFragmentSelector& selector,
    absl::optional<shared_highlighting::LinkGenerationError> error) {
  DCHECK(!preemptive_generation_result_.has_value());

  if (response_callback_) {
    InvokeReplyCallback(selector, error);
  } else {
    // If we don't have a callback yet, it's because we started generating
    // preemptively. We'll store the result so that when the selector actually
    // is requested we can simply use the stored result.
    preemptive_generation_result_.emplace(selector);
    error_ = error;
  }
}

void TextFragmentHandler::StartGeneratingForCurrentSelection() {
  if (GetFrame()->Selection().SelectedText().IsEmpty())
    return;

  VisibleSelectionInFlatTree selection =
      GetFrame()->Selection().ComputeVisibleSelectionInFlatTree();
  EphemeralRangeInFlatTree selection_range(selection.Start(), selection.End());
  RangeInFlatTree* current_selection_range =
      MakeGarbageCollected<RangeInFlatTree>(selection_range.StartPosition(),
                                            selection_range.EndPosition());
  GetTextFragmentSelectorGenerator()->Generate(
      *current_selection_range,
      WTF::Bind(&TextFragmentHandler::DidFinishSelectorGeneration,
                WrapWeakPersistent(this)));
}

void TextFragmentHandler::RecordPreemptiveGenerationMetrics(
    const TextFragmentSelector& selector,
    absl::optional<shared_highlighting::LinkGenerationError> optional_error) {
  DCHECK(selector_requested_before_ready_.has_value());

  bool success =
      selector.Type() != TextFragmentSelector::SelectorType::kInvalid;

  std::string uma_prefix = "SharedHighlights.LinkGenerated";
  if (selector_requested_before_ready_.value()) {
    uma_prefix = base::StrCat({uma_prefix, ".RequestedBeforeReady"});
  } else {
    uma_prefix = base::StrCat({uma_prefix, ".RequestedAfterReady"});
  }
  base::UmaHistogramBoolean(uma_prefix, success);

  if (!success) {
    shared_highlighting::LinkGenerationError error =
        optional_error.has_value()
            ? optional_error.value()
            : shared_highlighting::LinkGenerationError::kUnknown;
    base::UmaHistogramEnumeration(
        "SharedHighlights.LinkGenerated.Error.Requested", error);
  }
}

void TextFragmentHandler::StartPreemptiveGenerationIfNeeded() {
  preemptive_generation_result_.reset();
  error_.reset();
  selector_requested_before_ready_.reset();

  // It is possible we have unsurved callback, but if we are starting a new
  // generation, then we have a new selection, in which case it is safe to
  // assume that the client is not waiting for the callback return.
  response_callback_.Reset();

  if (!shared_highlighting::ShouldOfferLinkToText(
          GetFrame()->GetDocument()->Url())) {
    return;
  }

  StartGeneratingForCurrentSelection();
}

void TextFragmentHandler::Trace(Visitor* visitor) const {
  visitor->Trace(text_fragment_selector_generator_);
  visitor->Trace(selector_producer_);
}

void TextFragmentHandler::DidDetachDocumentOrFrame() {
  // Clear out any state in the generator and cancel pending tasks so they
  // don't run after frame detachment.
  GetTextFragmentSelectorGenerator()->Reset();
}

void TextFragmentHandler::InvokeReplyCallback(
    const TextFragmentSelector& selector,
    absl::optional<shared_highlighting::LinkGenerationError> error) {
  RecordPreemptiveGenerationMetrics(selector, error);

  DCHECK(response_callback_);
  std::move(response_callback_).Run(selector.ToString());
}

TextFragmentAnchor* TextFragmentHandler::GetTextFragmentAnchor() {
  if (!GetFrame() || !GetFrame()->View()) {
    return nullptr;
  }
  FragmentAnchor* fragmentAnchor = GetFrame()->View()->GetFragmentAnchor();
  if (!fragmentAnchor || !fragmentAnchor->IsTextFragmentAnchor()) {
    return nullptr;
  }
  return static_cast<TextFragmentAnchor*>(fragmentAnchor);
}

}  // namespace blink
