/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
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

#ifndef ElementRareData_h
#define ElementRareData_h

#include <memory>
#include "core/animation/ElementAnimations.h"
#include "core/css/cssom/InlineStylePropertyMap.h"
#include "core/dom/AccessibleNode.h"
#include "core/dom/Attr.h"
#include "core/dom/DOMTokenList.h"
#include "core/dom/DatasetDOMStringMap.h"
#include "core/dom/ElementIntersectionObserverData.h"
#include "core/dom/ElementShadow.h"
#include "core/dom/NamedNodeMap.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/PseudoElementData.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/custom/V0CustomElementDefinition.h"
#include "platform/bindings/ScriptWrappableVisitor.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashSet.h"

namespace blink {

class ResizeObservation;
class ResizeObserver;

class ElementRareData : public NodeRareData {
 public:
  static ElementRareData* Create(NodeRenderingData* node_layout_data) {
    return new ElementRareData(node_layout_data);
  }

  ~ElementRareData();

  void SetPseudoElement(PseudoId, PseudoElement*);
  PseudoElement* GetPseudoElement(PseudoId) const;

  void SetTabIndexExplicitly() {
    SetElementFlag(kTabIndexWasSetExplicitly, true);
  }

  void ClearTabIndexExplicitly() {
    ClearElementFlag(kTabIndexWasSetExplicitly);
  }

  CSSStyleDeclaration& EnsureInlineCSSStyleDeclaration(Element* owner_element);
  InlineStylePropertyMap& EnsureInlineStylePropertyMap(Element* owner_element);

  InlineStylePropertyMap* GetInlineStylePropertyMap() {
    return cssom_map_wrapper_.Get();
  }

  void ClearShadow() { shadow_ = nullptr; }
  ElementShadow* Shadow() const { return shadow_.Get(); }
  ElementShadow& EnsureShadow() {
    if (!shadow_) {
      shadow_ = ElementShadow::Create();
      ScriptWrappableVisitor::WriteBarrier(this, shadow_);
    }
    return *shadow_;
  }

  NamedNodeMap* AttributeMap() const { return attribute_map_.Get(); }
  void SetAttributeMap(NamedNodeMap* attribute_map) {
    attribute_map_ = attribute_map;
    ScriptWrappableVisitor::WriteBarrier(this, attribute_map_);
  }

  ComputedStyle* GetComputedStyle() const { return computed_style_.Get(); }
  void SetComputedStyle(PassRefPtr<ComputedStyle> computed_style) {
    computed_style_ = std::move(computed_style);
  }
  void ClearComputedStyle() { computed_style_ = nullptr; }

  DOMTokenList* GetClassList() const { return class_list_.Get(); }
  void SetClassList(DOMTokenList* class_list) {
    class_list_ = class_list;
    ScriptWrappableVisitor::WriteBarrier(this, class_list_);
  }

  DatasetDOMStringMap* Dataset() const { return dataset_.Get(); }
  void SetDataset(DatasetDOMStringMap* dataset) {
    dataset_ = dataset;
    ScriptWrappableVisitor::WriteBarrier(this, dataset_);
  }

  ScrollOffset SavedLayerScrollOffset() const {
    return saved_layer_scroll_offset_;
  }
  void SetSavedLayerScrollOffset(ScrollOffset offset) {
    saved_layer_scroll_offset_ = offset;
  }

  ElementAnimations* GetElementAnimations() {
    return element_animations_.Get();
  }
  void SetElementAnimations(ElementAnimations* element_animations) {
    element_animations_ = element_animations;
  }

  bool HasPseudoElements() const;
  void ClearPseudoElements();

  void V0SetCustomElementDefinition(V0CustomElementDefinition* definition) {
    v0_custom_element_definition_ = definition;
  }
  V0CustomElementDefinition* GetV0CustomElementDefinition() const {
    return v0_custom_element_definition_.Get();
  }

  void SetCustomElementDefinition(CustomElementDefinition* definition) {
    custom_element_definition_ = definition;
  }
  CustomElementDefinition* GetCustomElementDefinition() const {
    return custom_element_definition_.Get();
  }

  AccessibleNode* GetAccessibleNode() const { return accessible_node_.Get(); }
  AccessibleNode* EnsureAccessibleNode(Element* owner_element) {
    if (!accessible_node_) {
      accessible_node_ = new AccessibleNode(owner_element);
      ScriptWrappableVisitor::WriteBarrier(this, accessible_node_);
    }
    return accessible_node_;
  }

  AttrNodeList& EnsureAttrNodeList();
  AttrNodeList* GetAttrNodeList() { return attr_node_list_.Get(); }
  void RemoveAttrNodeList() { attr_node_list_.Clear(); }
  void AddAttr(Attr* attr) {
    EnsureAttrNodeList().push_back(attr);
    ScriptWrappableVisitor::WriteBarrier(this, attr);
  }

  ElementIntersectionObserverData* IntersectionObserverData() const {
    return intersection_observer_data_.Get();
  }
  ElementIntersectionObserverData& EnsureIntersectionObserverData() {
    if (!intersection_observer_data_) {
      intersection_observer_data_ = new ElementIntersectionObserverData();
      ScriptWrappableVisitor::WriteBarrier(this, intersection_observer_data_);
    }
    return *intersection_observer_data_;
  }

  using ResizeObserverDataMap =
      HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>;

  ResizeObserverDataMap* ResizeObserverData() const {
    return resize_observer_data_;
  }
  ResizeObserverDataMap& EnsureResizeObserverData();

  const AtomicString& GetNonce() const { return nonce_; }
  void SetNonce(const AtomicString& nonce) { nonce_ = nonce; }

  DECLARE_TRACE_AFTER_DISPATCH();
  DECLARE_TRACE_WRAPPERS_AFTER_DISPATCH();

 private:
  ScrollOffset saved_layer_scroll_offset_;
  AtomicString nonce_;

  Member<DatasetDOMStringMap> dataset_;
  Member<ElementShadow> shadow_;
  Member<DOMTokenList> class_list_;
  Member<NamedNodeMap> attribute_map_;
  Member<AttrNodeList> attr_node_list_;
  Member<InlineCSSStyleDeclaration> cssom_wrapper_;
  Member<InlineStylePropertyMap> cssom_map_wrapper_;

  Member<ElementAnimations> element_animations_;
  Member<ElementIntersectionObserverData> intersection_observer_data_;
  Member<ResizeObserverDataMap> resize_observer_data_;

  RefPtr<ComputedStyle> computed_style_;
  // TODO(davaajav):remove this field when v0 custom elements are deprecated
  Member<V0CustomElementDefinition> v0_custom_element_definition_;
  Member<CustomElementDefinition> custom_element_definition_;

  Member<PseudoElementData> pseudo_element_data_;

  Member<AccessibleNode> accessible_node_;

  explicit ElementRareData(NodeRenderingData*);
};
DEFINE_TRAIT_FOR_TRACE_WRAPPERS(ElementRareData);

inline LayoutSize DefaultMinimumSizeForResizing() {
  return LayoutSize(LayoutUnit::Max(), LayoutUnit::Max());
}

inline ElementRareData::ElementRareData(NodeRenderingData* node_layout_data)
    : NodeRareData(node_layout_data), class_list_(nullptr) {
  is_element_rare_data_ = true;
}

inline ElementRareData::~ElementRareData() {
  DCHECK(!pseudo_element_data_);
}

inline bool ElementRareData::HasPseudoElements() const {
  return (pseudo_element_data_ && pseudo_element_data_->HasPseudoElements());
}

inline void ElementRareData::ClearPseudoElements() {
  if (pseudo_element_data_) {
    pseudo_element_data_->ClearPseudoElements();
    pseudo_element_data_.Clear();
  }
}

inline void ElementRareData::SetPseudoElement(PseudoId pseudo_id,
                                              PseudoElement* element) {
  if (!pseudo_element_data_)
    pseudo_element_data_ = PseudoElementData::Create();
  pseudo_element_data_->SetPseudoElement(pseudo_id, element);
}

inline PseudoElement* ElementRareData::GetPseudoElement(
    PseudoId pseudo_id) const {
  if (!pseudo_element_data_)
    return nullptr;
  return pseudo_element_data_->GetPseudoElement(pseudo_id);
}

}  // namespace blink

#endif  // ElementRareData_h
