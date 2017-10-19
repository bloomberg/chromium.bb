/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGElementRareData_h
#define SVGElementRareData_h

#include "core/style/ComputedStyle.h"
#include "core/svg/SVGElement.h"
#include "platform/heap/Handle.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class SVGElementProxySet;

class SVGElementRareData
    : public GarbageCollectedFinalized<SVGElementRareData> {
  WTF_MAKE_NONCOPYABLE(SVGElementRareData);

 public:
  SVGElementRareData(SVGElement* owner)
      : owner_(owner),
        corresponding_element_(nullptr),
        instances_updates_blocked_(false),
        use_override_computed_style_(false),
        needs_override_computed_style_update_(false),
        web_animated_attributes_dirty_(false) {}

  SVGElementSet& OutgoingReferences() { return outgoing_references_; }
  const SVGElementSet& OutgoingReferences() const {
    return outgoing_references_;
  }
  SVGElementSet& IncomingReferences() { return incoming_references_; }
  const SVGElementSet& IncomingReferences() const {
    return incoming_references_;
  }

  SVGElementProxySet& EnsureElementProxySet();

  HeapHashSet<WeakMember<SVGElement>>& ElementInstances() {
    return element_instances_;
  }
  const HeapHashSet<WeakMember<SVGElement>>& ElementInstances() const {
    return element_instances_;
  }

  bool InstanceUpdatesBlocked() const { return instances_updates_blocked_; }
  void SetInstanceUpdatesBlocked(bool value) {
    instances_updates_blocked_ = value;
  }

  SVGElement* CorrespondingElement() const {
    return corresponding_element_.Get();
  }
  void SetCorrespondingElement(SVGElement* corresponding_element) {
    corresponding_element_ = corresponding_element;
  }

  void SetWebAnimatedAttributesDirty(bool dirty) {
    web_animated_attributes_dirty_ = dirty;
  }
  bool WebAnimatedAttributesDirty() const {
    return web_animated_attributes_dirty_;
  }

  HashSet<const QualifiedName*>& WebAnimatedAttributes() {
    return web_animated_attributes_;
  }

  MutableStylePropertySet* AnimatedSMILStyleProperties() const {
    return animated_smil_style_properties_.Get();
  }
  MutableStylePropertySet* EnsureAnimatedSMILStyleProperties();

  ComputedStyle* OverrideComputedStyle(Element*, const ComputedStyle*);

  bool UseOverrideComputedStyle() const { return use_override_computed_style_; }
  void SetUseOverrideComputedStyle(bool value) {
    use_override_computed_style_ = value;
  }
  void SetNeedsOverrideComputedStyleUpdate() {
    needs_override_computed_style_update_ = true;
  }

  AffineTransform* AnimateMotionTransform();

  void Trace(blink::Visitor*);

 private:
  Member<SVGElement> owner_;
  SVGElementSet outgoing_references_;
  SVGElementSet incoming_references_;
  HeapHashSet<WeakMember<SVGElement>> element_instances_;
  Member<SVGElementProxySet> element_proxy_set_;
  Member<SVGElement> corresponding_element_;
  bool instances_updates_blocked_ : 1;
  bool use_override_computed_style_ : 1;
  bool needs_override_computed_style_update_ : 1;
  bool web_animated_attributes_dirty_ : 1;
  HashSet<const QualifiedName*> web_animated_attributes_;
  Member<MutableStylePropertySet> animated_smil_style_properties_;
  RefPtr<ComputedStyle> override_computed_style_;
  // Used by <animateMotion>
  AffineTransform animate_motion_transform_;
};

}  // namespace blink

#endif
