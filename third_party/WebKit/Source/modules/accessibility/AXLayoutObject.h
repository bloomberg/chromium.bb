/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AXLayoutObject_h
#define AXLayoutObject_h

#include "modules/ModulesExport.h"
#include "modules/accessibility/AXNodeObject.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/wtf/Forward.h"

namespace blink {

class AXObjectCacheImpl;
class AXSVGRoot;
class Element;
class FrameView;
class HTMLAreaElement;
class IntPoint;
class Node;

class MODULES_EXPORT AXLayoutObject : public AXNodeObject {
  WTF_MAKE_NONCOPYABLE(AXLayoutObject);

 protected:
  AXLayoutObject(LayoutObject*, AXObjectCacheImpl&);

 public:
  static AXLayoutObject* Create(LayoutObject*, AXObjectCacheImpl&);
  ~AXLayoutObject() override;

  // Public, overridden from AXObjectImpl.
  LayoutObject* GetLayoutObject() const final { return layout_object_; }
  LayoutBoxModelObject* GetLayoutBoxModelObject() const;
  ScrollableArea* GetScrollableAreaIfScrollable() const final;
  AccessibilityRole DetermineAccessibilityRole() override;
  AccessibilityRole NativeAccessibilityRoleIgnoringAria() const override;

 protected:
  LayoutObject* layout_object_;

  LayoutObject* LayoutObjectForRelativeBounds() const override {
    return layout_object_;
  }

  //
  // Overridden from AXObjectImpl.
  //

  void Init() override;
  void Detach() override;
  bool IsDetached() const override { return !layout_object_; }
  bool IsAXLayoutObject() const override { return true; }

  // Check object role or purpose.
  bool IsEditable() const override;
  bool IsRichlyEditable() const override;
  bool IsLinked() const override;
  bool IsLoaded() const override;
  bool IsOffScreen() const override;
  bool IsReadOnly() const override;
  bool IsVisited() const override;

  // Check object state.
  bool IsFocused() const override;
  bool IsSelected() const override;

  // Whether objects are ignored, i.e. not included in the tree.
  AXObjectInclusion DefaultObjectInclusion(
      IgnoredReasons* = nullptr) const override;
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

  // Properties of static elements.
  const AtomicString& AccessKey() const override;
  RGBA32 ComputeBackgroundColor() const final;
  RGBA32 GetColor() const final;
  String FontFamily() const final;
  // Font size is in pixels.
  float FontSize() const final;
  String ImageDataUrl(const IntSize& max_size) const final;
  String GetText() const override;
  AccessibilityTextDirection GetTextDirection() const final;
  int TextLength() const override;
  TextStyle GetTextStyle() const final;
  KURL Url() const override;

  // Inline text boxes.
  void LoadInlineTextBoxes() override;
  AXObjectImpl* NextOnLine() const override;
  AXObjectImpl* PreviousOnLine() const override;

  // Properties of interactive elements.
  String StringValue() const override;

  // ARIA attributes.
  void AriaDescribedbyElements(AXObjectVector&) const override;
  void AriaLabelledbyElements(AXObjectVector&) const override;
  void AriaOwnsElements(AXObjectVector&) const override;

  bool AriaHasPopup() const override;
  bool AriaRoleHasPresentationalChildren() const override;
  AXObjectImpl* AncestorForWhichThisIsAPresentationalChild() const override;
  bool SupportsARIADragging() const override;
  bool SupportsARIADropping() const override;
  bool SupportsARIAFlowTo() const override;
  bool SupportsARIAOwns() const override;

  // ARIA live-region features.
  const AtomicString& LiveRegionStatus() const override;
  const AtomicString& LiveRegionRelevant() const override;
  bool LiveRegionAtomic() const override;
  bool LiveRegionBusy() const override;

  // AX name calc.
  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         AXNameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;

  // Methods that retrieve or manipulate the current selection.

  AXRange Selection() const override;
  AXRange SelectionUnderObject() const override;
  void SetSelection(const AXRange&) override;

  // Hit testing.
  AXObjectImpl* AccessibilityHitTest(const IntPoint&) const override;
  AXObjectImpl* ElementAccessibilityHitTest(const IntPoint&) const override;

  // High-level accessibility tree access. Other modules should only use these
  // functions.
  AXObjectImpl* ComputeParent() const override;
  AXObjectImpl* ComputeParentIfExists() const override;

  // Low-level accessibility tree exploration, only for use within the
  // accessibility module.
  AXObjectImpl* RawFirstChild() const override;
  AXObjectImpl* RawNextSibling() const override;
  void AddChildren() override;
  bool CanHaveChildren() const override;
  void UpdateChildrenIfNecessary() override;
  bool NeedsToUpdateChildren() const override { return children_dirty_; }
  void SetNeedsToUpdateChildren() override { children_dirty_ = true; }
  void ClearChildren() override;

  // Properties of the object's owning document or page.
  double EstimatedLoadingProgress() const override;

  // DOM and layout tree access.
  Node* GetNode() const override;
  Document* GetDocument() const override;
  FrameView* DocumentFrameView() const override;
  Element* AnchorElement() const override;

  void SetValue(const String&) override;

  // Notifications that this object may have changed.
  void HandleActiveDescendantChanged() override;
  void HandleAriaExpandedChanged() override;
  void TextChanged() override;

  // Text metrics. Most of these should be deprecated, needs major cleanup.
  int Index(const VisiblePosition&) const override;
  VisiblePosition VisiblePositionForIndex(int) const override;
  void LineBreaks(Vector<int>&) const final;

 private:
  AXObjectImpl* TreeAncestorDisallowingChild() const;
  bool IsTabItemSelected() const;
  bool IsValidSelectionBound(const AXObjectImpl*) const;
  AXObjectImpl* AccessibilityImageMapHitTest(HTMLAreaElement*,
                                             const IntPoint&) const;
  LayoutObject* LayoutParentObject() const;
  bool IsSVGImage() const;
  void DetachRemoteSVGRoot();
  AXSVGRoot* RemoteSVGRootElement() const;
  AXObjectImpl* RemoteSVGElementHitTest(const IntPoint&) const;
  void OffsetBoundingBoxForRemoteSVGElement(LayoutRect&) const;
  void AddHiddenChildren();
  void AddTextFieldChildren();
  void AddImageMapChildren();
  void AddCanvasChildren();
  void AddPopupChildren();
  void AddRemoteSVGChildren();
  void AddInlineTextBoxChildren(bool force);

  bool ElementAttributeValue(const QualifiedName&) const;
  LayoutRect ComputeElementRect() const;
  AXRange TextControlSelection() const;
  int IndexForVisiblePosition(const VisiblePosition&) const;
  AXLayoutObject* GetUnignoredObjectFromNode(Node&) const;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXLayoutObject, IsAXLayoutObject());

}  // namespace blink

#endif  // AXLayoutObject_h
