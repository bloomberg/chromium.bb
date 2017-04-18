/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
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

#ifndef LayoutPart_h
#define LayoutPart_h

#include "core/CoreExport.h"
#include "core/layout/LayoutReplaced.h"

namespace blink {

class FrameOrPlugin;
class PluginView;

// LayoutObject for frames via LayoutFrame and LayoutIFrame, and plugins via
// LayoutEmbeddedObject.
class CORE_EXPORT LayoutPart : public LayoutReplaced {
 public:
  explicit LayoutPart(Element*);
  ~LayoutPart() override;

  bool RequiresAcceleratedCompositing() const;

  bool NeedsPreferredWidthsRecalculation() const final;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation& location_in_container,
                   const LayoutPoint& accumulated_offset,
                   HitTestAction) override;

  void Ref() { ++ref_count_; }
  void Deref();

  // LayoutPart::ChildFrameView returns the FrameView associated with
  // the current Node, if Node is HTMLFrameOwnerElement.
  // This is different to LayoutObject::GetFrameView which returns
  // the FrameView associated with the root Document Frame.
  FrameView* ChildFrameView() const;
  PluginView* Plugin() const;
  FrameOrPlugin* GetFrameOrPlugin() const;

  LayoutRect ReplacedContentRect() const final;

  void UpdateOnWidgetChange();
  void UpdateGeometry();

  bool IsLayoutPart() const final { return true; }
  virtual void PaintContents(const PaintInfo&, const LayoutPoint&) const;

  bool IsThrottledFrameView() const;

 protected:
  PaintLayerType LayerTypeRequired() const override;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) final;
  void UpdateLayout() override;
  void Paint(const PaintInfo&, const LayoutPoint&) const override;
  CursorDirective GetCursor(const LayoutPoint&, Cursor&) const final;

  // Overridden to invalidate the child frame if any.
  void InvalidatePaintOfSubtreesIfNeeded(
      const PaintInvalidationState&) override;

 private:
  void UpdateGeometryInternal(FrameOrPlugin&);
  CompositingReasons AdditionalCompositingReasons() const override;

  void WillBeDestroyed() final;
  void Destroy() final;

  bool NodeAtPointOverFrameViewBase(
      HitTestResult&,
      const HitTestLocation& location_in_container,
      const LayoutPoint& accumulated_offset,
      HitTestAction);

  int ref_count_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutPart, IsLayoutPart());

}  // namespace blink

#endif  // LayoutPart_h
