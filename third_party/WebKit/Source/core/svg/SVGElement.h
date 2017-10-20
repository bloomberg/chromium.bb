/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2009, 2014 Apple Inc. All rights reserved.
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

#ifndef SVGElement_h
#define SVGElement_h

#include "core/CoreExport.h"
#include "core/dom/Element.h"
#include "core/svg/SVGParsingError.h"
#include "core/svg/properties/SVGPropertyInfo.h"
#include "core/svg_names.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class AffineTransform;
class Document;
class SVGAnimatedPropertyBase;
class SubtreeLayoutScope;
class SVGAnimatedString;
class SVGElement;
class SVGElementProxySet;
class SVGElementRareData;
class SVGPropertyBase;
class SVGSVGElement;
class SVGUseElement;

typedef HeapHashSet<Member<SVGElement>> SVGElementSet;

class CORE_EXPORT SVGElement : public Element {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~SVGElement() override;
  void AttachLayoutTree(AttachContext&) override;
  void DetachLayoutTree(const AttachContext&) override;

  int tabIndex() const override;
  bool SupportsFocus() const override { return false; }

  // The TreeScope this element should resolve id's against. This differs from
  // the regular Node::treeScope() by taking <use> into account.
  TreeScope& TreeScopeForIdResolution() const;

  bool IsOutermostSVGSVGElement() const;

  bool HasTagName(const SVGQualifiedName& name) const {
    return HasLocalName(name.LocalName());
  }

  String title() const override;
  bool HasRelativeLengths() const {
    return !elements_with_relative_lengths_.IsEmpty();
  }
  static bool IsAnimatableCSSProperty(const QualifiedName&);

  enum ApplyMotionTransform {
    kExcludeMotionTransform,
    kIncludeMotionTransform
  };
  bool HasTransform(ApplyMotionTransform) const;
  AffineTransform CalculateTransform(ApplyMotionTransform) const;

  enum CTMScope {
    kNearestViewportScope,  // Used by SVGGraphicsElement::getCTM()
    kScreenScope,           // Used by SVGGraphicsElement::getScreenCTM()
    kAncestorScope  // Used by SVGSVGElement::get{Enclosure|Intersection}List()
  };
  virtual AffineTransform LocalCoordinateSpaceTransform(CTMScope) const;
  virtual bool NeedsPendingResourceHandling() const { return true; }

  bool InstanceUpdatesBlocked() const;
  void SetInstanceUpdatesBlocked(bool);

  // Records the SVG element as having a Web Animation on an SVG attribute that
  // needs applying.
  void SetWebAnimationsPending();
  void ApplyActiveWebAnimations();

  void EnsureAttributeAnimValUpdated();

  void SetWebAnimatedAttribute(const QualifiedName& attribute,
                               SVGPropertyBase*);
  void ClearWebAnimatedAttributes();

  void SetAnimatedAttribute(const QualifiedName&, SVGPropertyBase*);
  void InvalidateAnimatedAttribute(const QualifiedName&);
  void ClearAnimatedAttribute(const QualifiedName&);

  SVGSVGElement* ownerSVGElement() const;
  SVGElement* viewportElement() const;

  virtual bool IsSVGGeometryElement() const { return false; }
  virtual bool IsSVGGraphicsElement() const { return false; }
  virtual bool IsFilterEffect() const { return false; }
  virtual bool IsTextContent() const { return false; }
  virtual bool IsTextPositioning() const { return false; }
  virtual bool IsStructurallyExternal() const { return false; }

  // For SVGTests
  virtual bool IsValid() const { return true; }

  virtual void SvgAttributeChanged(const QualifiedName&);
  void SvgAttributeBaseValChanged(const QualifiedName&);

  SVGAnimatedPropertyBase* PropertyFromAttribute(
      const QualifiedName& attribute_name) const;
  static AnimatedPropertyType AnimatedPropertyTypeForCSSAttribute(
      const QualifiedName& attribute_name);

  void SendSVGLoadEventToSelfAndAncestorChainIfPossible();
  bool SendSVGLoadEventIfPossible();

  virtual AffineTransform* AnimateMotionTransform() { return nullptr; }

  void InvalidateSVGAttributes() {
    EnsureUniqueElementData().animated_svg_attributes_are_dirty_ = true;
  }
  void InvalidateSVGPresentationAttributeStyle() {
    EnsureUniqueElementData().presentation_attribute_style_is_dirty_ = true;
  }

  const HeapHashSet<WeakMember<SVGElement>>& InstancesForElement() const;
  void MapInstanceToElement(SVGElement*);
  void RemoveInstanceMapping(SVGElement*);

  SVGElement* CorrespondingElement() const;
  void SetCorrespondingElement(SVGElement*);
  SVGUseElement* CorrespondingUseElement() const;

  void SynchronizeAnimatedSVGAttribute(const QualifiedName&) const;

  scoped_refptr<ComputedStyle> CustomStyleForLayoutObject() final;
  bool LayoutObjectIsNeeded(const ComputedStyle&) override;

#if DCHECK_IS_ON()
  virtual bool IsAnimatableAttribute(const QualifiedName&) const;
#endif

  MutableStylePropertySet* AnimatedSMILStyleProperties() const;
  MutableStylePropertySet* EnsureAnimatedSMILStyleProperties();
  void SetUseOverrideComputedStyle(bool);

  virtual bool HaveLoadedRequiredResources();

  void InvalidateRelativeLengthClients(SubtreeLayoutScope* = nullptr);

  void AddToPropertyMap(SVGAnimatedPropertyBase*);

  SVGAnimatedString* className() { return class_name_.Get(); }

  bool InUseShadowTree() const;

  SVGElementProxySet* ElementProxySet();

  SVGElementSet* SetOfIncomingReferences() const;
  void AddReferenceTo(SVGElement*);
  void RebuildAllIncomingReferences();
  void RemoveAllIncomingReferences();
  void RemoveAllOutgoingReferences();

  class InvalidationGuard {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(InvalidationGuard);

   public:
    InvalidationGuard(SVGElement* element) : element_(element) {}
    ~InvalidationGuard() { element_->InvalidateInstances(); }

   private:
    Member<SVGElement> element_;
  };

  class InstanceUpdateBlocker {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(InstanceUpdateBlocker);

   public:
    InstanceUpdateBlocker(SVGElement* target_element);
    ~InstanceUpdateBlocker();

   private:
    Member<SVGElement> target_element_;
  };

  void InvalidateInstances();
  void SetNeedsStyleRecalcForInstances(StyleChangeType,
                                       const StyleChangeReasonForTracing&);

  virtual void Trace(blink::Visitor*);

  static const AtomicString& EventParameterName();

  bool IsPresentationAttribute(const QualifiedName&) const override;
  virtual bool IsPresentationAttributeWithSVGDOM(const QualifiedName&) const;

 protected:
  SVGElement(const QualifiedName&,
             Document&,
             ConstructionType = kCreateSVGElement);

  void ParseAttribute(const AttributeModificationParams&) override;
  void AttributeChanged(const AttributeModificationParams&) override;

  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;

  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void RemovedFrom(ContainerNode*) override;
  void ChildrenChanged(const ChildrenChange&) override;

  static CSSPropertyID CssPropertyIdForSVGAttributeName(const QualifiedName&);
  void UpdateRelativeLengthsInformation() {
    UpdateRelativeLengthsInformation(SelfHasRelativeLengths(), this);
  }
  void UpdateRelativeLengthsInformation(bool has_relative_lengths, SVGElement*);
  static void MarkForLayoutAndParentResourceInvalidation(LayoutObject*);

  virtual bool SelfHasRelativeLengths() const { return false; }

  bool HasSVGParent() const;

  SVGElementRareData* EnsureSVGRareData();
  inline bool HasSVGRareData() const { return svg_rare_data_; }
  inline SVGElementRareData* SvgRareData() const {
    DCHECK(svg_rare_data_);
    return svg_rare_data_.Get();
  }

  void ReportAttributeParsingError(SVGParsingError,
                                   const QualifiedName&,
                                   const AtomicString&);
  bool HasFocusEventListeners() const;

  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) final;
  void RemovedEventListener(const AtomicString& event_type,
                            const RegisteredEventListener&) final;

 private:
  bool IsSVGElement() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsStyledElement() const =
      delete;  // This will catch anyone doing an unnecessary check.

  const ComputedStyle* EnsureComputedStyle(PseudoId = kPseudoIdNone);
  const ComputedStyle* VirtualEnsureComputedStyle(
      PseudoId pseudo_element_specifier = kPseudoIdNone) final {
    return EnsureComputedStyle(pseudo_element_specifier);
  }
  void WillRecalcStyle(StyleRecalcChange) override;

  void BuildPendingResourcesIfNeeded();

  HeapHashSet<WeakMember<SVGElement>> elements_with_relative_lengths_;

  typedef HeapHashMap<QualifiedName, Member<SVGAnimatedPropertyBase>>
      AttributeToPropertyMap;
  AttributeToPropertyMap attribute_to_property_map_;

#if DCHECK_IS_ON()
  bool in_relative_length_clients_invalidation_ = false;
#endif

  Member<SVGElementRareData> svg_rare_data_;
  Member<SVGAnimatedString> class_name_;
};

struct SVGAttributeHashTranslator {
  STATIC_ONLY(SVGAttributeHashTranslator);
  static unsigned GetHash(const QualifiedName& key) {
    if (key.HasPrefix()) {
      QualifiedNameComponents components = {g_null_atom.Impl(),
                                            key.LocalName().Impl(),
                                            key.NamespaceURI().Impl()};
      return HashComponents(components);
    }
    return DefaultHash<QualifiedName>::Hash::GetHash(key);
  }
  static bool Equal(const QualifiedName& a, const QualifiedName& b) {
    return a.Matches(b);
  }
};

DEFINE_ELEMENT_TYPE_CASTS(SVGElement, IsSVGElement());

template <typename T>
bool IsElementOfType(const SVGElement&);
template <>
inline bool IsElementOfType<const SVGElement>(const SVGElement&) {
  return true;
}

inline bool Node::HasTagName(const SVGQualifiedName& name) const {
  return IsSVGElement() && ToSVGElement(*this).HasTagName(name);
}

// This requires isSVG*Element(const SVGElement&).
#define DEFINE_SVGELEMENT_TYPE_CASTS_WITH_FUNCTION(thisType)               \
  inline bool Is##thisType(const thisType* element);                       \
  inline bool Is##thisType(const thisType& element);                       \
  inline bool Is##thisType(const SVGElement* element) {                    \
    return element && Is##thisType(*element);                              \
  }                                                                        \
  inline bool Is##thisType(const Node& node) {                             \
    return node.IsSVGElement() ? Is##thisType(ToSVGElement(node)) : false; \
  }                                                                        \
  inline bool Is##thisType(const Node* node) {                             \
    return node && Is##thisType(*node);                                    \
  }                                                                        \
  template <typename T>                                                    \
  inline bool Is##thisType(const T* node) {                                \
    return Is##thisType(node);                                             \
  }                                                                        \
  template <typename T>                                                    \
  inline bool Is##thisType(const Member<T>& node) {                        \
    return Is##thisType(node.Get());                                       \
  }                                                                        \
  template <>                                                              \
  inline bool IsElementOfType<const thisType>(const SVGElement& element) { \
    return Is##thisType(element);                                          \
  }                                                                        \
  DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(thisType)

}  // namespace blink

#include "core/svg_element_type_helpers.h"

#endif  // SVGElement_h
