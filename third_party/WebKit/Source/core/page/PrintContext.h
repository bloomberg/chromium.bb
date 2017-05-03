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

#ifndef PrintContext_h
#define PrintContext_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Element;
class FloatRect;
class FloatSize;
class GraphicsContext;
class IntRect;
class LocalFrame;
class Node;

class CORE_EXPORT PrintContext
    : public GarbageCollectedFinalized<PrintContext> {
 public:
  explicit PrintContext(LocalFrame*);
  virtual ~PrintContext();

  LocalFrame* GetFrame() const { return frame_; }

  // Break up a page into rects without relayout.
  // FIXME: This means that CSS page breaks won't be on page boundary if the
  // size is different than what was passed to BeginPrintMode(). That's probably
  // not always desirable.
  // FIXME: Header and footer height should be applied before layout, not after.
  // FIXME: The printRect argument is only used to determine page aspect ratio,
  // it would be better to pass a FloatSize with page dimensions instead.
  virtual void ComputePageRects(const FloatRect& print_rect,
                                float header_height,
                                float footer_height,
                                float user_scale_factor,
                                float& out_page_height);

  // Deprecated. Page size computation is already in this class, clients
  // shouldn't be copying it.
  virtual void ComputePageRectsWithPageSize(
      const FloatSize& page_size_in_pixels);

  // These are only valid after page rects are computed.
  size_t PageCount() const { return page_rects_.size(); }
  const IntRect& PageRect(size_t page_number) const {
    return page_rects_[page_number];
  }
  const Vector<IntRect>& PageRects() const { return page_rects_; }

  // Enter print mode, updating layout for new page size.
  // This function can be called multiple times to apply new print options
  // without going back to screen mode.
  virtual void BeginPrintMode(float width, float height = 0);

  // Return to screen mode.
  virtual void EndPrintMode();

  // Used by layout tests.
  static int PageNumberForElement(
      Element*,
      const FloatSize& page_size_in_pixels);  // Returns -1 if page isn't found.
  static String PageProperty(LocalFrame*,
                             const char* property_name,
                             int page_number);
  static bool IsPageBoxVisible(LocalFrame*, int page_number);
  static String PageSizeAndMarginsInPixels(LocalFrame*,
                                           int page_number,
                                           int width,
                                           int height,
                                           int margin_top,
                                           int margin_right,
                                           int margin_bottom,
                                           int margin_left);
  static int NumberOfPages(LocalFrame*, const FloatSize& page_size_in_pixels);

  DECLARE_VIRTUAL_TRACE();

 protected:
  friend class PrintContextTest;

  void OutputLinkedDestinations(GraphicsContext&, const IntRect& page_rect);

  Member<LocalFrame> frame_;
  Vector<IntRect> page_rects_;

 private:
  void ComputePageRectsWithPageSizeInternal(
      const FloatSize& page_size_in_pixels);
  void CollectLinkedDestinations(Node*);
  bool IsFrameValid() const;

  // Used to prevent misuses of BeginPrintMode() and EndPrintMode() (e.g., call
  // EndPrintMode without BeginPrintMode).
  bool is_printing_;

  HeapHashMap<String, Member<Element>> linked_destinations_;
  bool linked_destinations_valid_;
};

class ScopedPrintContext {
  STACK_ALLOCATED();

 public:
  explicit ScopedPrintContext(LocalFrame*);
  ~ScopedPrintContext();

  PrintContext* operator->() const { return context_.Get(); }

 private:
  Member<PrintContext> context_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPrintContext);
};

}  // namespace blink

#endif
