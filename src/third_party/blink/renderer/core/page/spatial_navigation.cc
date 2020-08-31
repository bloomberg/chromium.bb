/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Antonio Gomes <tonikitoo@webkit.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/spatial_navigation.h"

#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"

namespace blink {

// A small integer that easily fits into a double with a good margin for
// arithmetic. In particular, we don't want to use
// std::numeric_limits<double>::lowest() because, if subtracted, it becomes
// NaN which will make all following arithmetic NaN too (an unusable number).
constexpr double kMinDistance = std::numeric_limits<int>::lowest();
constexpr double kPriorityClassA = kMinDistance;
constexpr double kPriorityClassB = kMinDistance / 2;

constexpr int kFudgeFactor = 2;

FocusCandidate::FocusCandidate(Node* node, SpatialNavigationDirection direction)
    : visible_node(nullptr), focusable_node(nullptr), is_offscreen(true) {
  DCHECK(node);
  DCHECK(node->IsElementNode());

  if (auto* area = DynamicTo<HTMLAreaElement>(*node)) {
    HTMLImageElement* image = area->ImageElement();
    if (!image || !image->GetLayoutObject())
      return;

    visible_node = image;
    rect_in_root_frame = StartEdgeForAreaElement(*area, direction);
  } else {
    if (!node->GetLayoutObject())
      return;

    visible_node = node;
    rect_in_root_frame = NodeRectInRootFrame(node);
  }

  focusable_node = node;
  is_offscreen = IsOffscreen(visible_node);
}

bool IsSpatialNavigationEnabled(const LocalFrame* frame) {
  return (frame && frame->GetSettings() &&
          frame->GetSettings()->GetSpatialNavigationEnabled());
}

static bool RectsIntersectOnOrthogonalAxis(SpatialNavigationDirection direction,
                                           const PhysicalRect& a,
                                           const PhysicalRect& b) {
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
    case SpatialNavigationDirection::kRight:
      return a.Bottom() > b.Y() && a.Y() < b.Bottom();
    case SpatialNavigationDirection::kUp:
    case SpatialNavigationDirection::kDown:
      return a.Right() > b.X() && a.X() < b.Right();
    default:
      NOTREACHED();
      return false;
  }
}

// Return true if rect |a| is below |b|. False otherwise.
// For overlapping rects, |a| is considered to be below |b|
// if both edges of |a| are below the respective ones of |b|.
static inline bool Below(const PhysicalRect& a, const PhysicalRect& b) {
  return a.Y() >= b.Bottom() || (a.Y() >= b.Y() && a.Bottom() > b.Bottom() &&
                                 a.X() < b.Right() && a.Right() > b.X());
}

// Return true if rect |a| is on the right of |b|. False otherwise.
// For overlapping rects, |a| is considered to be on the right of |b|
// if both edges of |a| are on the right of the respective ones of |b|.
static inline bool RightOf(const PhysicalRect& a, const PhysicalRect& b) {
  return a.X() >= b.Right() || (a.X() >= b.X() && a.Right() > b.Right() &&
                                a.Y() < b.Bottom() && a.Bottom() > b.Y());
}

static bool IsRectInDirection(SpatialNavigationDirection direction,
                              const PhysicalRect& cur_rect,
                              const PhysicalRect& target_rect) {
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
      return RightOf(cur_rect, target_rect);
    case SpatialNavigationDirection::kRight:
      return RightOf(target_rect, cur_rect);
    case SpatialNavigationDirection::kUp:
      return Below(cur_rect, target_rect);
    case SpatialNavigationDirection::kDown:
      return Below(target_rect, cur_rect);
    default:
      NOTREACHED();
      return false;
  }
}

bool IsFragmentedInline(Node& node) {
  const LayoutObject* layout_object = node.GetLayoutObject();
  if (!layout_object->IsInline() || layout_object->IsAtomicInlineLevel())
    return false;

  // If it has empty quads, it's most likely not a fragmented text.
  // <a><div></div></a> has for example one empty rect.
  Vector<FloatQuad> quads;
  layout_object->AbsoluteQuads(quads);
  for (const FloatQuad& quad : quads) {
    if (quad.IsEmpty())
      return false;
  }

  return quads.size() > 1;
}

FloatRect RectInViewport(const Node& node) {
  LocalFrameView* frame_view = node.GetDocument().View();
  if (!frame_view)
    return FloatRect();

  DCHECK(!frame_view->NeedsLayout());

  LayoutObject* object = node.GetLayoutObject();
  if (!object)
    return FloatRect();

  PhysicalRect rect_in_root_frame = NodeRectInRootFrame(&node);

  // Convert to the visual viewport which will account for pinch zoom.
  VisualViewport& visual_viewport =
      object->GetDocument().GetPage()->GetVisualViewport();
  FloatRect rect_in_viewport =
      visual_viewport.RootFrameToViewport(FloatRect(rect_in_root_frame));

  // RootFrameToViewport doesn't clip so manually apply the viewport clip here.
  FloatRect viewport_rect =
      FloatRect(FloatPoint(), FloatSize(visual_viewport.Size()));
  rect_in_viewport.Intersect(viewport_rect);

  return rect_in_viewport;
}

// Answers true if |node| is completely outside the user's (visual) viewport.
// This logic is used by spatnav to rule out offscreen focus candidates and an
// offscreen activeElement. When activeElement is offscreen, spatnav doesn't use
// it as the search origin; the search will start at an edge of the visual
// viewport instead.
// TODO(crbug.com/889840): Fix VisibleBoundsInVisualViewport().
// If VisibleBoundsInVisualViewport() would have taken "element-clips" into
// account, spatnav could have called it directly; no need to check the
// LayoutObject's VisibleContentRect.
bool IsOffscreen(const Node* node) {
  DCHECK(node);
  return RectInViewport(*node).IsEmpty();
}

ScrollableArea* ScrollableAreaFor(const Node* node) {
  if (node->IsDocumentNode()) {
    LocalFrameView* view = node->GetDocument().View();
    if (!view)
      return nullptr;

    return view->GetScrollableArea();
  }

  LayoutObject* object = node->GetLayoutObject();
  if (!object || !object->IsBox())
    return nullptr;

  return ToLayoutBox(object)->GetScrollableArea();
}

bool IsUnobscured(const FocusCandidate& candidate) {
  DCHECK(candidate.visible_node);

  const LocalFrame* local_main_frame = DynamicTo<LocalFrame>(
      candidate.visible_node->GetDocument().GetPage()->MainFrame());
  if (!local_main_frame)
    return false;

  // TODO(crbug.com/955952): We cannot evaluate visibility for media element
  // using hit test since attached media controls cover media element.
  if (candidate.visible_node->IsMediaElement())
    return true;

  PhysicalRect viewport_rect(
      local_main_frame->GetPage()->GetVisualViewport().VisibleContentRect());
  PhysicalRect interesting_rect =
      Intersection(candidate.rect_in_root_frame, viewport_rect);

  if (interesting_rect.IsEmpty())
    return false;

  HitTestLocation location(interesting_rect);
  HitTestResult result =
      local_main_frame->GetEventHandler().HitTestResultAtLocation(
          location, HitTestRequest::kReadOnly | HitTestRequest::kListBased |
                        HitTestRequest::kIgnoreZeroOpacityObjects |
                        HitTestRequest::kAllowChildFrameContent);

  const HitTestResult::NodeSet& nodes = result.ListBasedTestResult();
  for (auto hit_node = nodes.rbegin(); hit_node != nodes.rend(); ++hit_node) {
    if (candidate.visible_node->ContainsIncludingHostElements(**hit_node))
      return true;

    if (FrameOwnerElement(candidate) &&
        FrameOwnerElement(candidate)
            ->contentDocument()
            ->ContainsIncludingHostElements(**hit_node))
      return true;
  }

  return false;
}

bool HasRemoteFrame(const Node* node) {
  auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(node);
  if (!frame_owner_element)
    return false;

  return frame_owner_element->ContentFrame() &&
         frame_owner_element->ContentFrame()->IsRemoteFrame();
}

bool ScrollInDirection(Node* container, SpatialNavigationDirection direction) {
  DCHECK(container);

  if (!CanScrollInDirection(container, direction))
    return false;

  int dx = 0;
  int dy = 0;
  int pixels_per_line_step =
      ScrollableArea::PixelsPerLineStep(container->GetDocument().GetFrame());
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
      dx = -pixels_per_line_step;
      break;
    case SpatialNavigationDirection::kRight:
      // TODO(bokan, https://crbug.com/952326): Fix this DCHECK.
      //  DCHECK_GT(container->GetLayoutBox()->ScrollWidth(),
      //            container->GetScrollableArea()->ScrollPosition().X() +
      //                container->GetLayoutBox()->ClientWidth());
      dx = pixels_per_line_step;
      break;
    case SpatialNavigationDirection::kUp:
      dy = -pixels_per_line_step;
      break;
    case SpatialNavigationDirection::kDown:
      // TODO(bokan, https://crbug.com/952326): Fix this DCHECK.
      //  DCHECK_GT(container->GetLayoutBox()->ScrollHeight(),
      //            container->GetScrollableArea()->ScrollPosition().Y() +
      //                container->GetLayoutBox()->ClientHeight());
      dy = pixels_per_line_step;
      break;
    default:
      NOTREACHED();
      return false;
  }

  // TODO(crbug.com/914775): Use UserScroll() instead. UserScroll() does a
  // smooth, animated scroll which might make it easier for users to understand
  // spatnav's moves. Another advantage of using ScrollableArea::UserScroll() is
  // that it returns a ScrollResult so we don't need to call
  // CanScrollInDirection(). Regular arrow-key scrolling (without
  // --enable-spatial-navigation) already uses smooth scrolling by default.
  ScrollableArea* scroller = ScrollableAreaFor(container);
  if (!scroller)
    return false;

  scroller->ScrollBy(ScrollOffset(dx, dy), mojom::blink::ScrollType::kUser);
  return true;
}

bool IsScrollableNode(const Node* node) {
  if (!node)
    return false;

  if (node->IsDocumentNode())
    return true;

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return false;

  return ToLayoutBox(layout_object)->CanBeScrolledAndHasScrollableArea();
}

Node* ScrollableAreaOrDocumentOf(Node* node) {
  DCHECK(node);
  Node* parent = node;
  do {
    // FIXME: Spatial navigation is broken for OOPI.
    if (auto* document = DynamicTo<Document>(parent))
      parent = document->GetFrame()->DeprecatedLocalOwner();
    else
      parent = parent->ParentOrShadowHostNode();
  } while (parent && !IsScrollableAreaOrDocument(parent));

  return parent;
}

bool IsScrollableAreaOrDocument(const Node* node) {
  if (!node)
    return false;

  auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(node);
  return (frame_owner_element && frame_owner_element->ContentFrame()) ||
         IsScrollableNode(node);
}

bool CanScrollInDirection(const Node* container,
                          SpatialNavigationDirection direction) {
  DCHECK(container);
  if (auto* document = DynamicTo<Document>(container))
    return CanScrollInDirection(document->GetFrame(), direction);

  if (!IsScrollableNode(container))
    return false;

  const Element* container_element = DynamicTo<Element>(container);
  if (!container_element)
    return false;
  auto* scrollable_area = container_element->GetScrollableArea();
  if (!scrollable_area)
    return false;

  DCHECK(container->GetLayoutObject());
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
      return (container->GetLayoutObject()->Style()->OverflowX() !=
                  EOverflow::kHidden &&
              scrollable_area->ScrollPosition().X() > 0);
    case SpatialNavigationDirection::kUp:
      return (container->GetLayoutObject()->Style()->OverflowY() !=
                  EOverflow::kHidden &&
              scrollable_area->ScrollPosition().Y() > 0);
    case SpatialNavigationDirection::kRight:
      return (container->GetLayoutObject()->Style()->OverflowX() !=
                  EOverflow::kHidden &&
              LayoutUnit(scrollable_area->ScrollPosition().X()) +
                      container->GetLayoutBox()->ClientWidth() <
                  container->GetLayoutBox()->ScrollWidth());
    case SpatialNavigationDirection::kDown:
      return (container->GetLayoutObject()->Style()->OverflowY() !=
                  EOverflow::kHidden &&
              LayoutUnit(scrollable_area->ScrollPosition().Y()) +
                      container->GetLayoutBox()->ClientHeight() <
                  container->GetLayoutBox()->ScrollHeight());
    default:
      NOTREACHED();
      return false;
  }
}

bool CanScrollInDirection(const LocalFrame* frame,
                          SpatialNavigationDirection direction) {
  if (!frame->View())
    return false;
  LayoutView* layoutView = frame->ContentLayoutObject();
  if (!layoutView)
    return false;
  mojom::blink::ScrollbarMode vertical_mode;
  mojom::blink::ScrollbarMode horizontal_mode;
  layoutView->CalculateScrollbarModes(horizontal_mode, vertical_mode);
  if ((direction == SpatialNavigationDirection::kLeft ||
       direction == SpatialNavigationDirection::kRight) &&
      mojom::blink::ScrollbarMode::kAlwaysOff == horizontal_mode)
    return false;
  if ((direction == SpatialNavigationDirection::kUp ||
       direction == SpatialNavigationDirection::kDown) &&
      mojom::blink::ScrollbarMode::kAlwaysOff == vertical_mode)
    return false;
  ScrollableArea* scrollable_area = frame->View()->GetScrollableArea();
  LayoutSize size(scrollable_area->ContentsSize());
  LayoutSize offset(scrollable_area->ScrollOffsetInt());
  PhysicalRect rect(scrollable_area->VisibleContentRect(kIncludeScrollbars));

  switch (direction) {
    case SpatialNavigationDirection::kLeft:
      return offset.Width() > 0;
    case SpatialNavigationDirection::kUp:
      return offset.Height() > 0;
    case SpatialNavigationDirection::kRight:
      return rect.Width() + offset.Width() < size.Width();
    case SpatialNavigationDirection::kDown:
      return rect.Height() + offset.Height() < size.Height();
    default:
      NOTREACHED();
      return false;
  }
}

PhysicalRect NodeRectInRootFrame(const Node* node) {
  DCHECK(node);
  DCHECK(node->GetLayoutObject());
  DCHECK(!node->GetDocument().View()->NeedsLayout());

  LayoutObject* object = node->GetLayoutObject();

  PhysicalRect rect = PhysicalRect::EnclosingRect(
      object->LocalBoundingBoxRectForAccessibility());

  // Inset the bounding box by the border.
  // TODO(bokan): As far as I can tell, this is to work around empty iframes
  // that have a border. It's unclear if that's still useful.
  rect.ContractEdges(LayoutUnit(object->StyleRef().BorderTopWidth()),
                     LayoutUnit(object->StyleRef().BorderRightWidth()),
                     LayoutUnit(object->StyleRef().BorderBottomWidth()),
                     LayoutUnit(object->StyleRef().BorderLeftWidth()));

  object->MapToVisualRectInAncestorSpace(/*ancestor=*/nullptr, rect);
  return rect;
}

// This method calculates the exitPoint from the startingRect and the entryPoint
// into the candidate rect.  The line between those 2 points is the closest
// distance between the 2 rects.  Takes care of overlapping rects, defining
// points so that the distance between them is zero where necessary.
void EntryAndExitPointsForDirection(SpatialNavigationDirection direction,
                                    const PhysicalRect& starting_rect,
                                    const PhysicalRect& potential_rect,
                                    LayoutPoint& exit_point,
                                    LayoutPoint& entry_point) {
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
      exit_point.SetX(starting_rect.X());
      if (potential_rect.Right() < starting_rect.X())
        entry_point.SetX(potential_rect.Right());
      else
        entry_point.SetX(starting_rect.X());
      break;
    case SpatialNavigationDirection::kUp:
      exit_point.SetY(starting_rect.Y());
      if (potential_rect.Bottom() < starting_rect.Y())
        entry_point.SetY(potential_rect.Bottom());
      else
        entry_point.SetY(starting_rect.Y());
      break;
    case SpatialNavigationDirection::kRight:
      exit_point.SetX(starting_rect.Right());
      if (potential_rect.X() > starting_rect.Right())
        entry_point.SetX(potential_rect.X());
      else
        entry_point.SetX(starting_rect.Right());
      break;
    case SpatialNavigationDirection::kDown:
      exit_point.SetY(starting_rect.Bottom());
      if (potential_rect.Y() > starting_rect.Bottom())
        entry_point.SetY(potential_rect.Y());
      else
        entry_point.SetY(starting_rect.Bottom());
      break;
    default:
      NOTREACHED();
  }

  switch (direction) {
    case SpatialNavigationDirection::kLeft:
    case SpatialNavigationDirection::kRight:
      if (Below(starting_rect, potential_rect)) {
        exit_point.SetY(starting_rect.Y());
        if (potential_rect.Bottom() < starting_rect.Y())
          entry_point.SetY(potential_rect.Bottom());
        else
          entry_point.SetY(starting_rect.Y());
      } else if (Below(potential_rect, starting_rect)) {
        exit_point.SetY(starting_rect.Bottom());
        if (potential_rect.Y() > starting_rect.Bottom())
          entry_point.SetY(potential_rect.Y());
        else
          entry_point.SetY(starting_rect.Bottom());
      } else {
        exit_point.SetY(max(starting_rect.Y(), potential_rect.Y()));
        entry_point.SetY(exit_point.Y());
      }
      break;
    case SpatialNavigationDirection::kUp:
    case SpatialNavigationDirection::kDown:
      if (RightOf(starting_rect, potential_rect)) {
        exit_point.SetX(starting_rect.X());
        if (potential_rect.Right() < starting_rect.X())
          entry_point.SetX(potential_rect.Right());
        else
          entry_point.SetX(starting_rect.X());
      } else if (RightOf(potential_rect, starting_rect)) {
        exit_point.SetX(starting_rect.Right());
        if (potential_rect.X() > starting_rect.Right())
          entry_point.SetX(potential_rect.X());
        else
          entry_point.SetX(starting_rect.Right());
      } else {
        exit_point.SetX(max(starting_rect.X(), potential_rect.X()));
        entry_point.SetX(exit_point.X());
      }
      break;
    default:
      NOTREACHED();
  }
}

double ProjectedOverlap(SpatialNavigationDirection direction,
                        PhysicalRect current,
                        PhysicalRect candidate) {
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
    case SpatialNavigationDirection::kRight:
      current.SetWidth(LayoutUnit(1));
      candidate.SetX(current.X());
      current.Intersect(candidate);
      return current.Height();
    case SpatialNavigationDirection::kUp:
    case SpatialNavigationDirection::kDown:
      current.SetHeight(LayoutUnit(1));
      candidate.SetY(current.Y());
      current.Intersect(candidate);
      return current.Width();
    default:
      NOTREACHED();
      return kMaxDistance;
  }
}

double Alignment(SpatialNavigationDirection direction,
                 PhysicalRect current,
                 PhysicalRect candidate) {
  // The formula and constants for "alignment" are experimental and
  // come from https://drafts.csswg.org/css-nav-1/#heuristics.
  const int kAlignWeight = 5;

  double projected_overlap = ProjectedOverlap(direction, current, candidate);
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
    case SpatialNavigationDirection::kRight:
      return (kAlignWeight * projected_overlap) / current.Height();
    case SpatialNavigationDirection::kUp:
    case SpatialNavigationDirection::kDown:
      return (kAlignWeight * projected_overlap) / current.Width();
    default:
      NOTREACHED();
      return kMaxDistance;
  }
}

bool BothOnTopmostPaintLayerInStackingContext(
    const FocusCandidate& current_interest,
    const FocusCandidate& candidate) {
  if (!current_interest.visible_node)
    return false;

  const LayoutObject* origin = current_interest.visible_node->GetLayoutObject();
  const PaintLayer* focused_layer = origin->PaintingLayer();
  if (!focused_layer || focused_layer->IsRootLayer())
    return false;

  const LayoutObject* next = candidate.visible_node->GetLayoutObject();
  const PaintLayer* candidate_layer = next->PaintingLayer();
  if (focused_layer != candidate_layer)
    return false;

  return !candidate_layer->HasVisibleDescendant();
}

double ComputeDistanceDataForNode(SpatialNavigationDirection direction,
                                  const FocusCandidate& current_interest,
                                  const FocusCandidate& candidate) {
  double distance = 0.0;
  PhysicalRect node_rect = candidate.rect_in_root_frame;
  PhysicalRect current_rect = current_interest.rect_in_root_frame;
  if (node_rect.Contains(current_rect)) {
    // When leaving an "insider", don't focus its underlaying container box.
    // Go directly to the outside world. This avoids focus from being trapped
    // inside a container.
    return kMaxDistance;
  }

  if (current_rect.Contains(node_rect)) {
    // We give highest priority to "insiders", candidates that are completely
    // inside the current focus rect, by giving them a negative, < 0, distance
    // number.
    distance = kPriorityClassA;

    // For insiders we cannot meassure the distance from the outer box. Instead,
    // we meassure distance _from_ the focused container's rect's "opposite
    // edge" in the navigated direction, just like we do when we look for
    // candidates inside a focused scroll container.
    current_rect = OppositeEdge(direction, current_rect);

    // This candidate fully overlaps the current focus rect so we can omit the
    // overlap term of the equation. An "insider" will always win against an
    // "outsider".
  } else if (!IsRectInDirection(direction, current_rect, node_rect)) {
    return kMaxDistance;
  } else if (BothOnTopmostPaintLayerInStackingContext(current_interest,
                                                      candidate)) {
    // Prioritize "popup candidates" over other candidates by giving them a
    // negative, < 0, distance number.
    distance = kPriorityClassB;
  }

  LayoutPoint exit_point;
  LayoutPoint entry_point;
  EntryAndExitPointsForDirection(direction, current_rect, node_rect, exit_point,
                                 entry_point);

  LayoutUnit x_axis = (exit_point.X() - entry_point.X()).Abs();
  LayoutUnit y_axis = (exit_point.Y() - entry_point.Y()).Abs();
  double euclidian_distance =
      sqrt((x_axis * x_axis + y_axis * y_axis).ToDouble());
  distance += euclidian_distance;

  LayoutUnit navigation_axis_distance;
  LayoutUnit weighted_orthogonal_axis_distance;

  // Bias and weights are put to the orthogonal axis distance calculation
  // so aligned candidates would have advantage over partially-aligned ones
  // and then over not-aligned candidates. The bias is given to not-aligned
  // candidates with respect to size of the current rect. The weight for
  // left/right direction is given a higher value to allow navigation on
  // common horizonally-aligned elements. The hardcoded values are based on
  // tests and experiments.
  const int kOrthogonalWeightForLeftRight = 30;
  const int kOrthogonalWeightForUpDown = 2;
  int orthogonal_bias = 0;

  switch (direction) {
    case SpatialNavigationDirection::kLeft:
    case SpatialNavigationDirection::kRight:
      navigation_axis_distance = x_axis;
      if (!RectsIntersectOnOrthogonalAxis(direction, current_rect, node_rect))
        orthogonal_bias = (current_rect.Height() / 2).ToInt();
      weighted_orthogonal_axis_distance =
          (y_axis + orthogonal_bias) * kOrthogonalWeightForLeftRight;
      break;
    case SpatialNavigationDirection::kUp:
    case SpatialNavigationDirection::kDown:
      navigation_axis_distance = y_axis;
      if (!RectsIntersectOnOrthogonalAxis(direction, current_rect, node_rect))
        orthogonal_bias = (current_rect.Width() / 2).ToInt();
      weighted_orthogonal_axis_distance =
          (x_axis + orthogonal_bias) * kOrthogonalWeightForUpDown;
      break;
    default:
      NOTREACHED();
      return kMaxDistance;
  }

  // We try to formalize this distance calculation at
  // https://drafts.csswg.org/css-nav-1/.
  distance += weighted_orthogonal_axis_distance.ToDouble() +
              navigation_axis_distance.ToDouble();
  return distance - Alignment(direction, current_rect, node_rect);
}

// Returns a thin rectangle that represents one of |box|'s edges.
// To not intersect elements that are positioned inside |box|, we add one
// LayoutUnit of margin that puts the returned slice "just outside" |box|.
PhysicalRect OppositeEdge(SpatialNavigationDirection side,
                          const PhysicalRect& box,
                          LayoutUnit thickness) {
  PhysicalRect thin_rect = box;
  switch (side) {
    case SpatialNavigationDirection::kLeft:
      thin_rect.SetX(thin_rect.Right() - thickness);
      thin_rect.SetWidth(thickness);
      thin_rect.offset.left += 1;
      break;
    case SpatialNavigationDirection::kRight:
      thin_rect.SetWidth(thickness);
      thin_rect.offset.left -= 1;
      break;
    case SpatialNavigationDirection::kDown:
      thin_rect.SetHeight(thickness);
      thin_rect.offset.top -= 1;
      break;
    case SpatialNavigationDirection::kUp:
      thin_rect.SetY(thin_rect.Bottom() - thickness);
      thin_rect.SetHeight(thickness);
      thin_rect.offset.top += 1;
      break;
    default:
      NOTREACHED();
  }

  return thin_rect;
}

PhysicalRect StartEdgeForAreaElement(const HTMLAreaElement& area,
                                     SpatialNavigationDirection direction) {
  DCHECK(area.ImageElement());
  // Area elements tend to overlap more than other focusable elements. We
  // flatten the rect of the area elements to minimize the effect of overlapping
  // areas.
  PhysicalRect rect = OppositeEdge(
      direction,
      area.GetDocument().GetFrame()->View()->ConvertToRootFrame(
          area.ComputeAbsoluteRect(area.ImageElement()->GetLayoutObject())),
      LayoutUnit(kFudgeFactor) /* snav-imagemap-overlapped-areas.html */);
  return rect;
}

HTMLFrameOwnerElement* FrameOwnerElement(const FocusCandidate& candidate) {
  return DynamicTo<HTMLFrameOwnerElement>(candidate.visible_node);
}

// The visual viewport's rect (given in the root frame's coordinate space).
PhysicalRect RootViewport(const LocalFrame* current_frame) {
  return PhysicalRect::EnclosingRect(
      current_frame->GetPage()->GetVisualViewport().VisibleRect());
}

// Ignores fragments that are completely offscreen.
// Returns the first one that is not offscreen, in the given iterator range.
template <class Iterator>
PhysicalRect FirstVisibleFragment(const PhysicalRect& visibility,
                                  Iterator fragment,
                                  Iterator end) {
  while (fragment != end) {
    PhysicalRect physical_fragment(EnclosedIntRect(fragment->BoundingBox()));
    physical_fragment.Intersect(visibility);
    if (!physical_fragment.IsEmpty())
      return physical_fragment;
    ++fragment;
  }
  return visibility;
}

PhysicalRect SearchOriginFragment(const PhysicalRect& visible_part,
                                  const LayoutObject& fragmented,
                                  const SpatialNavigationDirection direction) {
  // For accuracy, use the first visible fragment (not the fragmented element's
  // entire bounding rect which is a union of all fragments) as search origin.
  Vector<FloatQuad> fragments;
  fragmented.AbsoluteQuads(
      fragments, kTraverseDocumentBoundaries | kApplyRemoteRootFrameOffset);
  switch (direction) {
    case SpatialNavigationDirection::kLeft:
    case SpatialNavigationDirection::kDown:
      // Search from the topmost fragment.
      return FirstVisibleFragment(visible_part, fragments.begin(),
                                  fragments.end());
    case SpatialNavigationDirection::kRight:
    case SpatialNavigationDirection::kUp:
      // Search from the bottommost fragment.
      return FirstVisibleFragment(visible_part, fragments.rbegin(),
                                  fragments.rend());
    case SpatialNavigationDirection::kNone:
      break;
      // Nothing to do.
  }
  return visible_part;
}

// Spatnav uses this rectangle to measure distances to focus candidates.
// The search origin is either activeElement F itself, if it's being at least
// partially visible, or else, its first [partially] visible scroller. If both
// F and its enclosing scroller are completely off-screen, we recurse to the
// scroller’s scroller ... all the way up until the root frame's document.
// The root frame's document is a good base case because it's, per definition,
// a visible scrollable area.
PhysicalRect SearchOrigin(const PhysicalRect& viewport_rect_of_root_frame,
                          Node* focus_node,
                          const SpatialNavigationDirection direction) {
  if (!focus_node) {
    // Search from one of the visual viewport's edges towards the navigated
    // direction. For example, UP makes spatnav search upwards, starting at the
    // visual viewport's bottom.
    return OppositeEdge(direction, viewport_rect_of_root_frame);
  }

  auto* area_element = DynamicTo<HTMLAreaElement>(focus_node);
  if (area_element)
    focus_node = area_element->ImageElement();

  if (!IsOffscreen(focus_node)) {
    if (area_element)
      return StartEdgeForAreaElement(*area_element, direction);

    PhysicalRect box_in_root_frame = NodeRectInRootFrame(focus_node);
    PhysicalRect visible_part =
        Intersection(box_in_root_frame, viewport_rect_of_root_frame);

    if (IsFragmentedInline(*focus_node)) {
      return SearchOriginFragment(visible_part, *focus_node->GetLayoutObject(),
                                  direction);
    }
    return visible_part;
  }

  Node* container = ScrollableAreaOrDocumentOf(focus_node);
  while (container) {
    if (!IsOffscreen(container)) {
      // The first scroller that encloses focus and is [partially] visible.
      PhysicalRect box_in_root_frame = NodeRectInRootFrame(container);
      return OppositeEdge(direction, Intersection(box_in_root_frame,
                                                  viewport_rect_of_root_frame));
    }
    container = ScrollableAreaOrDocumentOf(container);
  }
  return OppositeEdge(direction, viewport_rect_of_root_frame);
}

}  // namespace blink
