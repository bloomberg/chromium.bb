/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann
 * <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGUseElement_h
#define SVGUseElement_h

#include "core/loader/resource/DocumentResource.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGGeometryElement.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGURIReference.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGUseElement final : public SVGGraphicsElement,
                            public SVGURIReference,
                            public DocumentResourceClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGUseElement);
  USING_PRE_FINALIZER(SVGUseElement, Dispose);

 public:
  static SVGUseElement* Create(Document&);
  ~SVGUseElement() override;

  void InvalidateShadowTree();

  // Return the element that should be used for clipping,
  // or null if a valid clip element is not directly referenced.
  SVGGraphicsElement* VisibleTargetGraphicsElementForClipping() const;

  SVGAnimatedLength* x() const { return x_.Get(); }
  SVGAnimatedLength* y() const { return y_.Get(); }
  SVGAnimatedLength* width() const { return width_.Get(); }
  SVGAnimatedLength* height() const { return height_.Get(); }

  void BuildPendingResource() override;
  String title() const override;

  void DispatchPendingEvent();
  void ToClipPath(Path&) const;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit SVGUseElement(Document&);

  void Dispose();

  FloatRect GetBBox() override;

  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;

  bool IsStructurallyExternal() const override;

  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void RemovedFrom(ContainerNode*) override;

  void SvgAttributeChanged(const QualifiedName&) override;

  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;

  void ScheduleShadowTreeRecreation();
  void CancelShadowTreeRecreation();
  bool HaveLoadedRequiredResources() override {
    return !IsStructurallyExternal() || have_fired_load_event_;
  }

  bool SelfHasRelativeLengths() const override;

  ShadowRoot& UseShadowRoot() const {
    CHECK(ClosedShadowRoot());
    return *ClosedShadowRoot();
  }

  // Instance tree handling
  enum ObserveBehavior {
    kAddObserver,
    kDontAddObserver,
  };
  Element* ResolveTargetElement(ObserveBehavior);
  void BuildShadowAndInstanceTree(SVGElement& target);
  void ClearInstanceRoot();
  Element* CreateInstanceTree(SVGElement& target_root) const;
  void ClearResourceReference();
  bool HasCycleUseReferencing(SVGUseElement&,
                              const ContainerNode& target_instance,
                              SVGElement*& new_target) const;
  bool ExpandUseElementsInShadowTree();
  void CloneNonMarkupEventListeners();
  void AddReferencesToFirstDegreeNestedUseElements(SVGElement& target);

  void InvalidateDependentShadowTrees();

  bool ResourceIsStillLoading() const;
  bool ResourceIsValid() const;
  bool InstanceTreeIsLoading() const;
  void NotifyFinished(Resource*) override;
  String DebugName() const override { return "SVGUseElement"; }
  void UpdateTargetReference();
  void SetDocumentResource(DocumentResource*);

  Member<SVGAnimatedLength> x_;
  Member<SVGAnimatedLength> y_;
  Member<SVGAnimatedLength> width_;
  Member<SVGAnimatedLength> height_;

  KURL element_url_;
  bool element_url_is_local_;
  bool have_fired_load_event_;
  bool needs_shadow_tree_recreation_;
  Member<SVGElement> target_element_instance_;
  Member<IdTargetObserver> target_id_observer_;
  Member<DocumentResource> resource_;
};

}  // namespace blink

#endif  // SVGUseElement_h
