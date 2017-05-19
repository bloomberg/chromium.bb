/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Apple Inc.
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
 */

#include "core/page/PrintContext.h"

#include <utility>

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutViewItem.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

namespace {

LayoutBoxModelObject* EnclosingBoxModelObject(LayoutObject* object) {
  while (object && !object->IsBoxModelObject())
    object = object->Parent();
  if (!object)
    return nullptr;
  return ToLayoutBoxModelObject(object);
}

bool IsCoordinateInPage(int top, int left, const IntRect& page) {
  return page.X() <= left && left < page.MaxX() && page.Y() <= top &&
         top < page.MaxY();
}

}  // namespace

PrintContext::PrintContext(LocalFrame* frame)
    : frame_(frame), is_printing_(false), linked_destinations_valid_(false) {}

PrintContext::~PrintContext() {
  DCHECK(!is_printing_);
}

void PrintContext::ComputePageRects(const FloatSize& print_size) {
  page_rects_.clear();

  if (!IsFrameValid())
    return;

  LayoutViewItem view = frame_->GetDocument()->GetLayoutViewItem();
  const IntRect& document_rect = view.DocumentRect();
  FloatSize page_size = frame_->ResizePageRectsKeepingRatio(
      print_size, FloatSize(document_rect.Width(), document_rect.Height()));
  ComputePageRectsWithPageSizeInternal(page_size);
}

void PrintContext::ComputePageRectsWithPageSize(
    const FloatSize& page_size_in_pixels) {
  page_rects_.clear();
  ComputePageRectsWithPageSizeInternal(page_size_in_pixels);
}

void PrintContext::ComputePageRectsWithPageSizeInternal(
    const FloatSize& page_size_in_pixels) {
  if (!IsFrameValid())
    return;

  LayoutViewItem view = frame_->GetDocument()->GetLayoutViewItem();

  IntRect doc_rect = view.DocumentRect();

  int page_width = page_size_in_pixels.Width();
  // We scaled with floating point arithmetic and need to ensure results like
  // 13329.99 are treated as 13330 so that we don't mistakenly assign an extra
  // page for the stray pixel.
  int page_height = page_size_in_pixels.Height() + LayoutUnit::Epsilon();

  bool is_horizontal = view.Style()->IsHorizontalWritingMode();

  int doc_logical_height = is_horizontal ? doc_rect.Height() : doc_rect.Width();
  int page_logical_height = is_horizontal ? page_height : page_width;
  int page_logical_width = is_horizontal ? page_width : page_height;

  int inline_direction_start = doc_rect.X();
  int inline_direction_end = doc_rect.MaxX();
  int block_direction_start = doc_rect.Y();
  int block_direction_end = doc_rect.MaxY();
  if (!is_horizontal) {
    std::swap(block_direction_start, inline_direction_start);
    std::swap(block_direction_end, inline_direction_end);
  }
  if (!view.Style()->IsLeftToRightDirection())
    std::swap(inline_direction_start, inline_direction_end);
  if (view.Style()->IsFlippedBlocksWritingMode())
    std::swap(block_direction_start, block_direction_end);

  unsigned page_count =
      ceilf(static_cast<float>(doc_logical_height) / page_logical_height);
  for (unsigned i = 0; i < page_count; ++i) {
    int page_logical_top =
        block_direction_end > block_direction_start
            ? block_direction_start + i * page_logical_height
            : block_direction_start - (i + 1) * page_logical_height;

    int page_logical_left = inline_direction_end > inline_direction_start
                                ? inline_direction_start
                                : inline_direction_start - page_logical_width;
    IntRect page_rect(page_logical_left, page_logical_top, page_logical_width,
                      page_logical_height);
    if (!is_horizontal)
      page_rect = page_rect.TransposedRect();
    page_rects_.push_back(page_rect);
  }
}

void PrintContext::BeginPrintMode(float width, float height) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);

  // This function can be called multiple times to adjust printing parameters
  // without going back to screen mode.
  is_printing_ = true;

  FloatSize original_page_size = FloatSize(width, height);
  FloatSize min_layout_size = frame_->ResizePageRectsKeepingRatio(
      original_page_size, FloatSize(width * kPrintingMinimumShrinkFactor,
                                    height * kPrintingMinimumShrinkFactor));

  // This changes layout, so callers need to make sure that they don't paint to
  // screen while in printing mode.
  frame_->SetPrinting(
      true, min_layout_size, original_page_size,
      kPrintingMaximumShrinkFactor / kPrintingMinimumShrinkFactor);
}

void PrintContext::EndPrintMode() {
  DCHECK(is_printing_);
  is_printing_ = false;
  if (IsFrameValid())
    frame_->SetPrinting(false, FloatSize(), FloatSize(), 0);
  linked_destinations_.clear();
  linked_destinations_valid_ = false;
}

// static
int PrintContext::PageNumberForElement(Element* element,
                                       const FloatSize& page_size_in_pixels) {
  element->GetDocument().UpdateStyleAndLayout();

  LocalFrame* frame = element->GetDocument().GetFrame();
  FloatRect page_rect(FloatPoint(0, 0), page_size_in_pixels);
  ScopedPrintContext print_context(frame);
  print_context->BeginPrintMode(page_rect.Width(), page_rect.Height());

  LayoutBoxModelObject* box =
      EnclosingBoxModelObject(element->GetLayoutObject());
  if (!box)
    return -1;

  FloatSize scaled_page_size = page_size_in_pixels;
  scaled_page_size.Scale(frame->View()->ContentsSize().Width() /
                         page_rect.Width());
  print_context->ComputePageRectsWithPageSize(scaled_page_size);

  int top = box->PixelSnappedOffsetTop(box->OffsetParent());
  int left = box->PixelSnappedOffsetLeft(box->OffsetParent());
  for (size_t page_number = 0; page_number < print_context->PageCount();
       ++page_number) {
    if (IsCoordinateInPage(top, left, print_context->PageRect(page_number)))
      return page_number;
  }
  return -1;
}

void PrintContext::CollectLinkedDestinations(Node* node) {
  for (Node* i = node->firstChild(); i; i = i->nextSibling())
    CollectLinkedDestinations(i);

  if (!node->IsLink() || !node->IsElementNode())
    return;
  const AtomicString& href = ToElement(node)->getAttribute(HTMLNames::hrefAttr);
  if (href.IsNull())
    return;
  KURL url = node->GetDocument().CompleteURL(href);
  if (!url.IsValid())
    return;

  if (url.HasFragmentIdentifier() &&
      EqualIgnoringFragmentIdentifier(url, node->GetDocument().BaseURL())) {
    String name = url.FragmentIdentifier();
    if (Element* element = node->GetDocument().FindAnchor(name))
      linked_destinations_.Set(name, element);
  }
}

void PrintContext::OutputLinkedDestinations(GraphicsContext& context,
                                            const IntRect& page_rect) {
  if (!linked_destinations_valid_) {
    // Collect anchors in the top-level frame only because our PrintContext
    // supports only one namespace for the anchors.
    CollectLinkedDestinations(GetFrame()->GetDocument());
    linked_destinations_valid_ = true;
  }

  for (const auto& entry : linked_destinations_) {
    LayoutObject* layout_object = entry.value->GetLayoutObject();
    if (!layout_object || !layout_object->GetFrameView())
      continue;
    IntRect bounding_box = layout_object->AbsoluteBoundingBoxRect();
    // TODO(bokan): |bounding_box| looks to be in content coordinates but
    // ConvertToRootFrame() doesn't apply scroll offsets when converting up to
    // the root frame.
    IntPoint point = layout_object->GetFrameView()->ConvertToRootFrame(
        bounding_box.Location());
    if (!page_rect.Contains(point))
      continue;
    point.ClampNegativeToZero();
    context.SetURLDestinationLocation(entry.key, point);
  }
}

// static
String PrintContext::PageProperty(LocalFrame* frame,
                                  const char* property_name,
                                  int page_number) {
  Document* document = frame->GetDocument();
  ScopedPrintContext print_context(frame);
  // Any non-zero size is OK here. We don't care about actual layout. We just
  // want to collect @page rules and figure out what declarations apply on a
  // given page (that may or may not exist).
  print_context->BeginPrintMode(800, 1000);
  RefPtr<ComputedStyle> style = document->StyleForPage(page_number);

  // Implement formatters for properties we care about.
  if (!strcmp(property_name, "margin-left")) {
    if (style->MarginLeft().IsAuto())
      return String("auto");
    return String::Number(style->MarginLeft().Value());
  }
  if (!strcmp(property_name, "line-height"))
    return String::Number(style->LineHeight().Value());
  if (!strcmp(property_name, "font-size"))
    return String::Number(style->GetFontDescription().ComputedPixelSize());
  if (!strcmp(property_name, "font-family"))
    return style->GetFontDescription().Family().Family().GetString();
  if (!strcmp(property_name, "size"))
    return String::Number(style->PageSize().Width()) + ' ' +
           String::Number(style->PageSize().Height());

  return String("pageProperty() unimplemented for: ") + property_name;
}

bool PrintContext::IsPageBoxVisible(LocalFrame* frame, int page_number) {
  return frame->GetDocument()->IsPageBoxVisible(page_number);
}

String PrintContext::PageSizeAndMarginsInPixels(LocalFrame* frame,
                                                int page_number,
                                                int width,
                                                int height,
                                                int margin_top,
                                                int margin_right,
                                                int margin_bottom,
                                                int margin_left) {
  DoubleSize page_size(width, height);
  frame->GetDocument()->PageSizeAndMarginsInPixels(page_number, page_size,
                                                   margin_top, margin_right,
                                                   margin_bottom, margin_left);

  return "(" + String::Number(floor(page_size.Width())) + ", " +
         String::Number(floor(page_size.Height())) + ") " +
         String::Number(margin_top) + ' ' + String::Number(margin_right) + ' ' +
         String::Number(margin_bottom) + ' ' + String::Number(margin_left);
}

// static
int PrintContext::NumberOfPages(LocalFrame* frame,
                                const FloatSize& page_size_in_pixels) {
  frame->GetDocument()->UpdateStyleAndLayout();

  FloatRect page_rect(FloatPoint(0, 0), page_size_in_pixels);
  ScopedPrintContext print_context(frame);
  print_context->BeginPrintMode(page_rect.Width(), page_rect.Height());
  // Account for shrink-to-fit.
  FloatSize scaled_page_size = page_size_in_pixels;
  scaled_page_size.Scale(frame->View()->ContentsSize().Width() /
                         page_rect.Width());
  print_context->ComputePageRectsWithPageSize(scaled_page_size);
  return print_context->PageCount();
}

bool PrintContext::IsFrameValid() const {
  return frame_->View() && frame_->GetDocument() &&
         !frame_->GetDocument()->GetLayoutViewItem().IsNull();
}

DEFINE_TRACE(PrintContext) {
  visitor->Trace(frame_);
  visitor->Trace(linked_destinations_);
}

ScopedPrintContext::ScopedPrintContext(LocalFrame* frame)
    : context_(new PrintContext(frame)) {}

ScopedPrintContext::~ScopedPrintContext() {
  context_->EndPrintMode();
}

}  // namespace blink
