/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/layout/LayoutEmbeddedContent.h"

#include "core/dom/AXObjectCache.h"
#include "core/frame/EmbeddedContentView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/RemoteFrameView.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutAPIShim.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/scrolling/RootScrollerUtil.h"
#include "core/paint/EmbeddedContentPainter.h"
#include "core/plugins/PluginView.h"

namespace blink {

LayoutEmbeddedContent::LayoutEmbeddedContent(Element* element)
    : LayoutReplaced(element),
      // Reference counting is used to prevent the part from being destroyed
      // while inside the EmbeddedContentView code, which might not be able to
      // handle that.
      ref_count_(1) {
  DCHECK(element);
  SetInline(false);
}

void LayoutEmbeddedContent::Deref() {
  if (--ref_count_ <= 0)
    delete this;
}

void LayoutEmbeddedContent::WillBeDestroyed() {
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
    cache->ChildrenChanged(this->Parent());
    cache->Remove(this);
  }

  Node* node = GetNode();
  if (node && node->IsFrameOwnerElement())
    ToHTMLFrameOwnerElement(node)->SetEmbeddedContentView(nullptr);

  LayoutReplaced::WillBeDestroyed();
}

void LayoutEmbeddedContent::Destroy() {
  WillBeDestroyed();
  // We call clearNode here because LayoutEmbeddedContent is ref counted. This
  // call to destroy may not actually destroy the layout object. We can keep it
  // around because of references from the LocalFrameView class. (The actual
  // destruction of the class happens in PostDestroy() which is called from
  // Deref()).
  //
  // But, we've told the system we've destroyed the layoutObject, which happens
  // when the DOM node is destroyed. So there is a good change the DOM node this
  // object points too is invalid, so we have to clear the node so we make sure
  // we don't access it in the future.
  ClearNode();
  Deref();
}

LayoutEmbeddedContent::~LayoutEmbeddedContent() {
  DCHECK_LE(ref_count_, 0);
}

LocalFrameView* LayoutEmbeddedContent::ChildFrameView() const {
  EmbeddedContentView* embedded_content_view = GetEmbeddedContentView();
  if (embedded_content_view && embedded_content_view->IsLocalFrameView())
    return ToLocalFrameView(embedded_content_view);
  return nullptr;
}

PluginView* LayoutEmbeddedContent::Plugin() const {
  EmbeddedContentView* embedded_content_view = GetEmbeddedContentView();
  if (embedded_content_view && embedded_content_view->IsPluginView())
    return ToPluginView(embedded_content_view);
  return nullptr;
}

EmbeddedContentView* LayoutEmbeddedContent::GetEmbeddedContentView() const {
  Node* node = GetNode();
  if (node && node->IsFrameOwnerElement())
    return ToHTMLFrameOwnerElement(node)->OwnedEmbeddedContentView();
  return nullptr;
}

PaintLayerType LayoutEmbeddedContent::LayerTypeRequired() const {
  PaintLayerType type = LayoutReplaced::LayerTypeRequired();
  if (type != kNoPaintLayer)
    return type;
  return kForcedPaintLayer;
}

bool LayoutEmbeddedContent::RequiresAcceleratedCompositing() const {
  // There are two general cases in which we can return true. First, if this is
  // a plugin LayoutObject and the plugin has a layer, then we need a layer.
  // Second, if this is a LayoutObject with a contentDocument and that document
  // needs a layer, then we need a layer.
  PluginView* plugin_view = Plugin();
  if (plugin_view && plugin_view->PlatformLayer())
    return true;

  if (!GetNode() || !GetNode()->IsFrameOwnerElement())
    return false;

  HTMLFrameOwnerElement* element = ToHTMLFrameOwnerElement(GetNode());
  if (element->ContentFrame() && element->ContentFrame()->IsRemoteFrame())
    return true;

  if (Document* content_document = element->contentDocument()) {
    LayoutViewItem view_item = content_document->GetLayoutViewItem();
    if (!view_item.IsNull())
      return view_item.UsesCompositing();
  }

  return false;
}

bool LayoutEmbeddedContent::NeedsPreferredWidthsRecalculation() const {
  if (LayoutReplaced::NeedsPreferredWidthsRecalculation())
    return true;
  return EmbeddedReplacedContent();
}

bool LayoutEmbeddedContent::NodeAtPointOverEmbeddedContentView(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset,
    HitTestAction action) {
  bool had_result = result.InnerNode();
  bool inside = LayoutReplaced::NodeAtPoint(result, location_in_container,
                                            accumulated_offset, action);

  // Check to see if we are really over the EmbeddedContentView itself (and not
  // just in the border/padding area).
  if ((inside || result.IsRectBasedTest()) && !had_result &&
      result.InnerNode() == GetNode()) {
    result.SetIsOverEmbeddedContentView(
        ContentBoxRect().Contains(result.LocalPoint()));
  }
  return inside;
}

bool LayoutEmbeddedContent::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset,
    HitTestAction action) {
  LocalFrameView* frame_view = ChildFrameView();
  if (!frame_view || !result.GetHitTestRequest().AllowsChildFrameContent()) {
    return NodeAtPointOverEmbeddedContentView(result, location_in_container,
                                              accumulated_offset, action);
  }

  // A hit test can never hit an off-screen element; only off-screen iframes are
  // throttled; therefore, hit tests can skip descending into throttled iframes.
  if (frame_view->ShouldThrottleRendering()) {
    return NodeAtPointOverEmbeddedContentView(result, location_in_container,
                                              accumulated_offset, action);
  }

  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kCompositingClean);

  if (action == kHitTestForeground) {
    LayoutViewItem child_root_item = frame_view->GetLayoutViewItem();

    if (VisibleToHitTestRequest(result.GetHitTestRequest()) &&
        !child_root_item.IsNull()) {
      LayoutPoint adjusted_location = accumulated_offset + Location();
      LayoutPoint content_offset = LayoutPoint(BorderLeft() + PaddingLeft(),
                                               BorderTop() + PaddingTop()) -
                                   LayoutSize(frame_view->ScrollOffsetInt());
      HitTestLocation new_hit_test_location(
          location_in_container, -adjusted_location - content_offset);
      HitTestRequest new_hit_test_request(result.GetHitTestRequest().GetType() |
                                          HitTestRequest::kChildFrameHitTest);
      HitTestResult child_frame_result(new_hit_test_request,
                                       new_hit_test_location);

      // The frame's layout and style must be up to date if we reach here.
      bool is_inside_child_frame =
          child_root_item.HitTestNoLifecycleUpdate(child_frame_result);

      if (result.GetHitTestRequest().ListBased()) {
        result.Append(child_frame_result);
      } else if (is_inside_child_frame) {
        // Force the result not to be cacheable because the parent frame should
        // not cache this result; as it won't be notified of changes in the
        // child.
        child_frame_result.SetCacheable(false);
        result = child_frame_result;
      }

      // Don't trust |isInsideChildFrame|. For rect-based hit-test, returns
      // true only when the hit test rect is totally within the iframe,
      // i.e. nodeAtPointOverEmbeddedContentView() also returns true.
      // Use a temporary HitTestResult because we don't want to collect the
      // iframe element itself if the hit-test rect is totally within the
      // iframe.
      if (is_inside_child_frame) {
        if (!location_in_container.IsRectBasedTest())
          return true;
        HitTestResult point_over_embedded_content_view_result = result;
        bool point_over_embedded_content_view =
            NodeAtPointOverEmbeddedContentView(
                point_over_embedded_content_view_result, location_in_container,
                accumulated_offset, action);
        if (point_over_embedded_content_view)
          return true;
        result = point_over_embedded_content_view_result;
        return false;
      }
    }
  }

  return NodeAtPointOverEmbeddedContentView(result, location_in_container,
                                            accumulated_offset, action);
}

CompositingReasons LayoutEmbeddedContent::AdditionalCompositingReasons() const {
  if (RequiresAcceleratedCompositing())
    return kCompositingReasonIFrame;
  return kCompositingReasonNone;
}

void LayoutEmbeddedContent::StyleDidChange(StyleDifference diff,
                                           const ComputedStyle* old_style) {
  LayoutReplaced::StyleDidChange(diff, old_style);
  EmbeddedContentView* embedded_content_view = GetEmbeddedContentView();
  if (!embedded_content_view)
    return;

  // If the iframe has custom scrollbars, recalculate their style.
  if (LocalFrameView* frame_view = ChildFrameView())
    frame_view->RecalculateCustomScrollbarStyle();

  if (Style()->Visibility() != EVisibility::kVisible) {
    embedded_content_view->Hide();
  } else {
    embedded_content_view->Show();
  }
}

void LayoutEmbeddedContent::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);
  UpdateAfterLayout();
  ClearNeedsLayout();
}

void LayoutEmbeddedContent::Paint(const PaintInfo& paint_info,
                                  const LayoutPoint& paint_offset) const {
  EmbeddedContentPainter(*this).Paint(paint_info, paint_offset);
}

void LayoutEmbeddedContent::PaintContents(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) const {
  EmbeddedContentPainter(*this).PaintContents(paint_info, paint_offset);
}

CursorDirective LayoutEmbeddedContent::GetCursor(const LayoutPoint& point,
                                                 Cursor& cursor) const {
  if (Plugin()) {
    // A plugin is responsible for setting the cursor when the pointer is over
    // it.
    return kDoNotSetCursor;
  }
  return LayoutReplaced::GetCursor(point, cursor);
}

LayoutRect LayoutEmbeddedContent::ReplacedContentRect() const {
  // We don't propagate sub-pixel into sub-frame layout, in other words, the
  // rect is snapped at the document boundary, and sub-pixel movement could
  // cause the sub-frame to layout due to the 1px snap difference. In order to
  // avoid that, the size of sub-frame is rounded in advance.
  LayoutRect size_rounded_rect = ContentBoxRect();

  // IFrames set as the root scroller should get their size from their parent.
  if (ChildFrameView() && View() && RootScrollerUtil::IsEffective(*this))
    size_rounded_rect = LayoutRect(LayoutPoint(), View()->ViewRect().Size());

  size_rounded_rect.SetSize(
      LayoutSize(RoundedIntSize(size_rounded_rect.Size())));
  return size_rounded_rect;
}

void LayoutEmbeddedContent::UpdateOnEmbeddedContentViewChange() {
  EmbeddedContentView* embedded_content_view = GetEmbeddedContentView();
  if (!embedded_content_view)
    return;

  if (!Style())
    return;

  if (!NeedsLayout())
    UpdateGeometryInternal(*embedded_content_view);

  if (Style()->Visibility() != EVisibility::kVisible) {
    embedded_content_view->Hide();
  } else {
    embedded_content_view->Show();
    // FIXME: Why do we issue a full paint invalidation in this case, but not
    // the other?
    SetShouldDoFullPaintInvalidation();
  }
}

void LayoutEmbeddedContent::UpdateGeometry() {
  EmbeddedContentView* embedded_content_view = GetEmbeddedContentView();
  if (!embedded_content_view)
    return;

  LayoutRect new_frame = ReplacedContentRect();
  DCHECK(new_frame.Size() == RoundedIntSize(new_frame.Size()));
  bool bounds_will_change =
      LayoutSize(embedded_content_view->FrameRect().Size()) != new_frame.Size();

  // If frame bounds are changing mark the view for layout. Also check the
  // frame's page to make sure that the frame isn't in the process of being
  // destroyed. If iframe scrollbars needs reconstruction from native to custom
  // scrollbar, then also we need to layout the frameview.
  LocalFrameView* frame_view = ChildFrameView();
  if (frame_view && frame_view->GetFrame().GetPage() &&
      (bounds_will_change || frame_view->NeedsScrollbarReconstruction()))
    frame_view->SetNeedsLayout();

  UpdateGeometryInternal(*embedded_content_view);

  // If view needs layout, either because bounds have changed or possibly
  // indicating content size is wrong, we have to do a layout to set the right
  // LocalFrameView size.
  if (frame_view && frame_view->NeedsLayout() &&
      frame_view->GetFrame().GetPage())
    frame_view->UpdateLayout();

  if (PluginView* plugin = Plugin())
    plugin->GeometryMayHaveChanged();
}

void LayoutEmbeddedContent::UpdateGeometryInternal(
    EmbeddedContentView& embedded_content_view) {
  // Ignore transform here, as we only care about the sub-pixel accumulation.
  // TODO(trchen): What about multicol? Need a LayoutBox function to query
  // sub-pixel accumulation.
  LayoutPoint absolute_location(LocalToAbsolute(FloatPoint()));
  LayoutRect absolute_replaced_rect = ReplacedContentRect();
  absolute_replaced_rect.MoveBy(absolute_location);

  IntRect frame_rect(IntPoint(),
                     PixelSnappedIntRect(absolute_replaced_rect).Size());
  // Normally the location of the frame rect is ignored by the painter, but
  // currently it is still used by a family of coordinate conversion function in
  // LocalFrameView. This is incorrect because coordinate conversion
  // needs to take transform and into account. A few callers still use the
  // family of conversion function, including but not exhaustive:
  // LocalFrameView::updateViewportIntersectionIfNeeded()
  // RemoteFrameView::frameRectsChanged().
  // WebPluginContainerImpl::reportGeometry()
  // TODO(trchen): Remove this hack once we fixed all callers.
  FloatRect absolute_bounding_box =
      LocalToAbsoluteQuad(FloatRect(ReplacedContentRect())).BoundingBox();
  frame_rect.SetLocation(RoundedIntPoint(absolute_bounding_box.Location()));

  // Why is the protector needed?
  RefPtr<LayoutEmbeddedContent> protector(this);
  embedded_content_view.SetFrameRect(frame_rect);
}

bool LayoutEmbeddedContent::IsThrottledFrameView() const {
  if (LocalFrameView* frame_view = ChildFrameView())
    return frame_view->ShouldThrottleRendering();
  return false;
}

}  // namespace blink
