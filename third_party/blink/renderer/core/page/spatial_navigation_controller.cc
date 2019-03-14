// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"

#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"

namespace blink {

namespace {

SpatialNavigationDirection FocusDirectionForKey(KeyboardEvent* event) {
  if (event->ctrlKey() || event->metaKey() || event->shiftKey())
    return SpatialNavigationDirection::kNone;

  SpatialNavigationDirection ret_val = SpatialNavigationDirection::kNone;
  if (event->key() == "ArrowDown")
    ret_val = SpatialNavigationDirection::kDown;
  else if (event->key() == "ArrowUp")
    ret_val = SpatialNavigationDirection::kUp;
  else if (event->key() == "ArrowLeft")
    ret_val = SpatialNavigationDirection::kLeft;
  else if (event->key() == "ArrowRight")
    ret_val = SpatialNavigationDirection::kRight;

  // TODO(bokan): We should probably assert that we don't get anything else but
  // currently KeyboardEventManager sends non-arrow keys here.

  return ret_val;
}

void ClearFocusInExitedFrames(LocalFrame* old_frame,
                              const LocalFrame* const new_frame) {
  while (old_frame && new_frame != old_frame) {
    // Focus is going away from this document, so clear the focused element.
    old_frame->GetDocument()->ClearFocusedElement();
    old_frame->GetDocument()->SetSequentialFocusNavigationStartingPoint(
        nullptr);
    Frame* parent = old_frame->Tree().Parent();
    old_frame = DynamicTo<LocalFrame>(parent);
  }
}

// Determines whether the given candidate is closer to the current interested
// node (in the given direction) than the current best. If so, it'll replace
// the current best.
static void ConsiderForBestCandidate(SpatialNavigationDirection direction,
                                     const FocusCandidate& current_interest,
                                     const FocusCandidate& candidate,
                                     FocusCandidate* best_candidate,
                                     double* best_distance) {
  DCHECK(candidate.visible_node->IsElementNode());
  DCHECK(candidate.visible_node->GetLayoutObject());

  // Ignore iframes that don't have a src attribute
  if (FrameOwnerElement(candidate) &&
      (!FrameOwnerElement(candidate)->ContentFrame() ||
       candidate.rect_in_root_frame.IsEmpty()))
    return;

  // Ignore off-screen focusables that are not exposed after one "scroll step"
  // in the direction.
  if (candidate.is_offscreen && candidate.is_offscreen_after_scrolling)
    return;

  double distance =
      ComputeDistanceDataForNode(direction, current_interest, candidate);
  if (distance == MaxDistance())
    return;

  if (best_candidate->IsNull()) {
    *best_candidate = candidate;
    *best_distance = distance;
    return;
  }

  LayoutRect intersection_rect = Intersection(
      candidate.rect_in_root_frame, best_candidate->rect_in_root_frame);
  if (!intersection_rect.IsEmpty() &&
      !AreElementsOnSameLine(*best_candidate, candidate) &&
      intersection_rect == candidate.rect_in_root_frame) {
    // If 2 nodes are intersecting, do hit test to find which node in on top.
    LayoutUnit x = intersection_rect.X() + intersection_rect.Width() / 2;
    LayoutUnit y = intersection_rect.Y() + intersection_rect.Height() / 2;
    if (!IsA<LocalFrame>(
            candidate.visible_node->GetDocument().GetPage()->MainFrame()))
      return;
    HitTestLocation location(IntPoint(x.ToInt(), y.ToInt()));
    HitTestResult result =
        candidate.visible_node->GetDocument()
            .GetPage()
            ->DeprecatedLocalMainFrame()
            ->GetEventHandler()
            .HitTestResultAtLocation(
                location, HitTestRequest::kReadOnly | HitTestRequest::kActive |
                              HitTestRequest::kIgnoreClipping);
    if (candidate.visible_node->contains(result.InnerNode())) {
      *best_candidate = candidate;
      *best_distance = distance;
      return;
    }
    if (best_candidate->visible_node->contains(result.InnerNode()))
      return;
  }

  if (distance < *best_distance) {
    *best_candidate = candidate;
    *best_distance = distance;
  }
}

}  // namespace

// static
SpatialNavigationController* SpatialNavigationController::Create(Page& page) {
  return MakeGarbageCollected<SpatialNavigationController>(page);
}

SpatialNavigationController::SpatialNavigationController(Page& page)
    : page_(&page) {}

bool SpatialNavigationController::HandleArrowKeyboardEvent(
    KeyboardEvent* event) {
  DCHECK(page_->GetSettings().GetSpatialNavigationEnabled());

  // TODO(bokan): KeyboardEventManager sends non-arrow keys here. KEM should
  // filter out the non-arrow keys for us.
  SpatialNavigationDirection direction = FocusDirectionForKey(event);
  if (direction == SpatialNavigationDirection::kNone)
    return false;

  // In focusless mode, the user must explicitly move focus in and out of an
  // editable so we can avoid advancing interest and we should swallow the
  // event. This prevents double-handling actions for things like search box
  // suggestions.
  if (RuntimeEnabledFeatures::FocuslessSpatialNavigationEnabled()) {
    LocalFrame* frame =
        DynamicTo<LocalFrame>(page_->GetFocusController().FocusedOrMainFrame());
    if (frame->Selection().SelectionHasFocus())
      return true;
  }

  return Advance(direction);
}

bool SpatialNavigationController::HandleEnterKeyboardEvent(
    KeyboardEvent* event) {
  DCHECK(page_->GetSettings().GetSpatialNavigationEnabled());

  if (interest_element_) {
    interest_element_->focus(FocusParams(SelectionBehaviorOnFocus::kReset,
                                         kWebFocusTypeSpatialNavigation,
                                         nullptr));
  }

  return true;
}

bool SpatialNavigationController::HandleEscapeKeyboardEvent(
    KeyboardEvent* event) {
  DCHECK(page_->GetSettings().GetSpatialNavigationEnabled());

  if (!interest_element_)
    return false;

  if (interest_element_->IsFocusedElementInDocument())
    interest_element_->blur();
  else
    MoveInterestTo(nullptr);

  return true;
}

Element* SpatialNavigationController::GetInterestedElement() const {
  if (RuntimeEnabledFeatures::FocuslessSpatialNavigationEnabled())
    return interest_element_;

  Frame* frame = page_->GetFocusController().FocusedOrMainFrame();
  auto* local_frame = DynamicTo<LocalFrame>(frame);
  if (!local_frame)
    return nullptr;

  Document* document = local_frame->GetDocument();
  if (!document)
    return nullptr;

  return document->ActiveElement();
}

void SpatialNavigationController::DidDetachFrameView() {
  // If the interested element's view was lost (frame detached, navigated,
  // etc.) then reset navigation.
  if (interest_element_ && !interest_element_->GetDocument().View())
    interest_element_ = nullptr;
}

void SpatialNavigationController::Trace(blink::Visitor* visitor) {
  visitor->Trace(interest_element_);
  visitor->Trace(page_);
}

bool SpatialNavigationController::Advance(
    SpatialNavigationDirection direction) {
  Node* interest_node = StartingNode();
  if (!interest_node)
    return false;

  interest_node->GetDocument().UpdateStyleAndLayout();

  Node* container = ScrollableAreaOrDocumentOf(interest_node);

  const LayoutRect visible_rect(page_->GetVisualViewport().VisibleRect());
  const LayoutRect start_box =
      SearchOrigin(visible_rect, interest_node, direction);

  if (IsScrollableAreaOrDocument(interest_node) &&
      !IsOffscreen(interest_node)) {
    // A visible scroller has interest. Search inside of it from one of its
    // edges.
    LayoutRect edge = OppositeEdge(direction, start_box);
    if (AdvanceWithinContainer(*interest_node, edge, direction, nullptr))
      return true;
  }

  // The interested scroller had nothing. Let's search outside of it.
  Node* skipped_tree = interest_node;
  while (container) {
    if (AdvanceWithinContainer(*container, start_box, direction, skipped_tree))
      return true;

    // Containers are not focused “on the way out”. This prevents containers
    // from acting as “focus traps”. Take <c> <a> </c> <b>. Focus can move from
    // <a> to <b> but not from <a> to the scroll container <c>. If we'd allow
    // focus to move from <a> to <c>, the user would never be able to exit <c>.
    // When the scroll container <c> is focused, we move focus back to <a>...
    skipped_tree = container;
    // Nothing found in |container| so search the parent container.
    container = ScrollableAreaOrDocumentOf(container);

    // TODO(bokan): This needs to update the parent document when the _current_
    // container is a document since we're crossing the document boundary.
    // Currently this will fail if we're going from an inner document to a
    // sub-scroller in a parent document.
    if (auto* document = DynamicTo<Document>(container))
      document->UpdateStyleAndLayout();
  }

  return false;
}

FocusCandidate SpatialNavigationController::FindNextCandidateInContainer(
    Node& container,
    const LayoutRect& starting_rect_in_root_frame,
    SpatialNavigationDirection direction,
    Node* interest_child_in_container) {
  Element* element = ElementTraversal::FirstWithin(container);

  FocusCandidate current_interest;
  current_interest.rect_in_root_frame = starting_rect_in_root_frame;
  current_interest.focusable_node = interest_child_in_container;
  current_interest.visible_node = interest_child_in_container;

  FocusCandidate best_candidate;
  double best_distance = MaxDistance();
  for (; element;
       element =
           IsScrollableAreaOrDocument(element)
               ? ElementTraversal::NextSkippingChildren(*element, &container)
               : ElementTraversal::Next(*element, &container)) {
    if (element == interest_child_in_container)
      continue;

    if (HasRemoteFrame(element))
      continue;

    if (!IsValidCandidate(*element))
      continue;

    FocusCandidate candidate = FocusCandidate(element, direction);
    if (candidate.IsNull())
      continue;

    ConsiderForBestCandidate(direction, current_interest, candidate,
                             &best_candidate, &best_distance);
  }

  return best_candidate;
}

bool SpatialNavigationController::AdvanceWithinContainer(
    Node& container,
    const LayoutRect& starting_rect_in_root_frame,
    SpatialNavigationDirection direction,
    Node* interest_child_in_container) {
  DCHECK(IsScrollableAreaOrDocument(&container));

  FocusCandidate candidate =
      FindNextCandidateInContainer(container, starting_rect_in_root_frame,
                                   direction, interest_child_in_container);

  if (candidate.IsNull()) {
    // Nothing to focus in this container, scroll if possible.
    // NOTE: If no scrolling is performed (i.e. ScrollInDirection returns
    // false), the spatial navigation algorithm will skip this container.
    return ScrollInDirection(&container, direction);
  }

  Element* element = ToElement(candidate.focusable_node);
  DCHECK(element);
  MoveInterestTo(element);
  return true;
}

Node* SpatialNavigationController::StartingNode() {
  if (RuntimeEnabledFeatures::FocuslessSpatialNavigationEnabled()) {
    if (interest_element_ && interest_element_->isConnected() &&
        interest_element_->GetDocument().GetFrame()) {
      // If an iframe is interested, start the search from its document node.
      // This matches the behavior in the focus case below where focusing a
      // frame means the focused document doesn't have a focused element and so
      // we return the document itself.
      if (auto* frame_owner =
              DynamicTo<HTMLFrameOwnerElement>(interest_element_.Get()))
        return frame_owner->contentDocument();

      return interest_element_;
    }

    if (auto* main_local_frame = DynamicTo<LocalFrame>(page_->MainFrame()))
      return main_local_frame->GetDocument();

    return nullptr;
  }

  // FIXME: Directional focus changes don't yet work with RemoteFrames.
  const auto* current_frame =
      DynamicTo<LocalFrame>(page_->GetFocusController().FocusedOrMainFrame());
  if (!current_frame)
    return nullptr;

  Document* focused_document = current_frame->GetDocument();
  if (!focused_document)
    return nullptr;

  Node* focused_element = focused_document->FocusedElement();
  if (!focused_element)  // An iframe's document is focused.
    focused_element = focused_document;

  return focused_element;
}

void SpatialNavigationController::MoveInterestTo(Node* next_node) {
  DCHECK(!next_node || next_node->IsElementNode());
  Element* element = ToElement(next_node);

  if (RuntimeEnabledFeatures::FocuslessSpatialNavigationEnabled()) {
    if (interest_element_) {
      interest_element_->blur();
      interest_element_->SetNeedsStyleRecalc(
          kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                 style_change_reason::kPseudoClass));
    }

    interest_element_ = element;

    if (interest_element_) {
      interest_element_->SetNeedsStyleRecalc(
          kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                 style_change_reason::kPseudoClass));

      LayoutObject* layout_object = interest_element_->GetLayoutObject();
      DCHECK(layout_object);

      layout_object->ScrollRectToVisible(
          element->BoundingBoxForScrollIntoView(), WebScrollIntoViewParams());
    }
    return;
  }

  if (!element)
    return;

  // Before focusing the new element, check if we're leaving an iframe (= moving
  // focus out of an iframe). In this case, we want the exited [nested] iframes
  // to lose focus. This is tested in snav-iframe-nested.html.
  LocalFrame* old_frame = page_->GetFocusController().FocusedFrame();
  ClearFocusInExitedFrames(old_frame, next_node->GetDocument().GetFrame());

  element->focus(FocusParams(SelectionBehaviorOnFocus::kReset,
                             kWebFocusTypeSpatialNavigation, nullptr));
}

bool SpatialNavigationController::IsValidCandidate(
    const Element& element) const {
  return element.IsKeyboardFocusable();
}

}  // namespace blink
