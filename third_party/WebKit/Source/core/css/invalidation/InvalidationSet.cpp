/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/invalidation/InvalidationSet.h"

#include <memory>
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

static const unsigned char* g_tracing_enabled = nullptr;

#define TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED( \
    element, reason, invalidationSet, singleSelectorPart)             \
  if (UNLIKELY(*g_tracing_enabled))                                   \
    TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART(                \
        element, reason, invalidationSet, singleSelectorPart);

void InvalidationSet::CacheTracingFlag() {
  g_tracing_enabled = TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"));
}

InvalidationSet::InvalidationSet(InvalidationType type)
    : ref_count_(1),
      type_(type),
      all_descendants_might_be_invalid_(false),
      invalidates_self_(false),
      custom_pseudo_invalid_(false),
      tree_boundary_crossing_(false),
      insertion_point_crossing_(false),
      invalidates_slotted_(false),
      is_alive_(true) {}

bool InvalidationSet::InvalidatesElement(Element& element) const {
  if (all_descendants_might_be_invalid_)
    return true;

  if (tag_names_ && tag_names_->Contains(element.TagQName().LocalName())) {
    TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(
        element, kInvalidationSetMatchedTagName, *this,
        element.TagQName().LocalName());
    return true;
  }

  if (element.HasID() && ids_ &&
      ids_->Contains(element.IdForStyleResolution())) {
    TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(
        element, kInvalidationSetMatchedId, *this,
        element.IdForStyleResolution());
    return true;
  }

  if (element.HasClass() && classes_) {
    const SpaceSplitString& class_names = element.ClassNames();
    for (const auto& class_name : *classes_) {
      if (class_names.Contains(class_name)) {
        TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(
            element, kInvalidationSetMatchedClass, *this, class_name);
        return true;
      }
    }
  }

  if (element.hasAttributes() && attributes_) {
    for (const auto& attribute : *attributes_) {
      if (element.hasAttribute(attribute)) {
        TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(
            element, kInvalidationSetMatchedAttribute, *this, attribute);
        return true;
      }
    }
  }

  return false;
}

bool InvalidationSet::InvalidatesTagName(Element& element) const {
  if (tag_names_ && tag_names_->Contains(element.TagQName().LocalName())) {
    TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(
        element, kInvalidationSetMatchedTagName, *this,
        element.TagQName().LocalName());
    return true;
  }

  return false;
}

void InvalidationSet::Combine(const InvalidationSet& other) {
  CHECK(is_alive_);
  CHECK(other.is_alive_);
  CHECK_NE(&other, this);
  CHECK_EQ(GetType(), other.GetType());
  if (GetType() == kInvalidateSiblings) {
    SiblingInvalidationSet& siblings = ToSiblingInvalidationSet(*this);
    const SiblingInvalidationSet& other_siblings =
        ToSiblingInvalidationSet(other);

    siblings.UpdateMaxDirectAdjacentSelectors(
        other_siblings.MaxDirectAdjacentSelectors());
    if (other_siblings.SiblingDescendants())
      siblings.EnsureSiblingDescendants().Combine(
          *other_siblings.SiblingDescendants());
    if (other_siblings.Descendants())
      siblings.EnsureDescendants().Combine(*other_siblings.Descendants());
  }

  if (other.InvalidatesSelf())
    SetInvalidatesSelf();

  // No longer bother combining data structures, since the whole subtree is
  // deemed invalid.
  if (WholeSubtreeInvalid())
    return;

  if (other.WholeSubtreeInvalid()) {
    SetWholeSubtreeInvalid();
    return;
  }

  if (other.CustomPseudoInvalid())
    SetCustomPseudoInvalid();

  if (other.TreeBoundaryCrossing())
    SetTreeBoundaryCrossing();

  if (other.InsertionPointCrossing())
    SetInsertionPointCrossing();

  if (other.InvalidatesSlotted())
    SetInvalidatesSlotted();

  if (other.classes_) {
    for (const auto& class_name : *other.classes_)
      AddClass(class_name);
  }

  if (other.ids_) {
    for (const auto& id : *other.ids_)
      AddId(id);
  }

  if (other.tag_names_) {
    for (const auto& tag_name : *other.tag_names_)
      AddTagName(tag_name);
  }

  if (other.attributes_) {
    for (const auto& attribute : *other.attributes_)
      AddAttribute(attribute);
  }
}

void InvalidationSet::Destroy() {
  if (IsDescendantInvalidationSet())
    delete ToDescendantInvalidationSet(this);
  else
    delete ToSiblingInvalidationSet(this);
}

HashSet<AtomicString>& InvalidationSet::EnsureClassSet() {
  if (!classes_)
    classes_ = WTF::WrapUnique(new HashSet<AtomicString>);
  return *classes_;
}

HashSet<AtomicString>& InvalidationSet::EnsureIdSet() {
  if (!ids_)
    ids_ = WTF::WrapUnique(new HashSet<AtomicString>);
  return *ids_;
}

HashSet<AtomicString>& InvalidationSet::EnsureTagNameSet() {
  if (!tag_names_)
    tag_names_ = WTF::WrapUnique(new HashSet<AtomicString>);
  return *tag_names_;
}

HashSet<AtomicString>& InvalidationSet::EnsureAttributeSet() {
  if (!attributes_)
    attributes_ = WTF::WrapUnique(new HashSet<AtomicString>);
  return *attributes_;
}

void InvalidationSet::AddClass(const AtomicString& class_name) {
  if (WholeSubtreeInvalid())
    return;
  CHECK(!class_name.IsEmpty());
  EnsureClassSet().insert(class_name);
}

void InvalidationSet::AddId(const AtomicString& id) {
  if (WholeSubtreeInvalid())
    return;
  CHECK(!id.IsEmpty());
  EnsureIdSet().insert(id);
}

void InvalidationSet::AddTagName(const AtomicString& tag_name) {
  if (WholeSubtreeInvalid())
    return;
  CHECK(!tag_name.IsEmpty());
  EnsureTagNameSet().insert(tag_name);
}

void InvalidationSet::AddAttribute(const AtomicString& attribute) {
  if (WholeSubtreeInvalid())
    return;
  CHECK(!attribute.IsEmpty());
  EnsureAttributeSet().insert(attribute);
}

void InvalidationSet::SetWholeSubtreeInvalid() {
  if (all_descendants_might_be_invalid_)
    return;

  all_descendants_might_be_invalid_ = true;
  custom_pseudo_invalid_ = false;
  tree_boundary_crossing_ = false;
  insertion_point_crossing_ = false;
  invalidates_slotted_ = false;
  classes_ = nullptr;
  ids_ = nullptr;
  tag_names_ = nullptr;
  attributes_ = nullptr;
}

void InvalidationSet::ToTracedValue(TracedValue* value) const {
  value->BeginDictionary();

  value->SetString("id", DescendantInvalidationSetToIdString(*this));

  if (all_descendants_might_be_invalid_)
    value->SetBoolean("allDescendantsMightBeInvalid", true);
  if (custom_pseudo_invalid_)
    value->SetBoolean("customPseudoInvalid", true);
  if (tree_boundary_crossing_)
    value->SetBoolean("treeBoundaryCrossing", true);
  if (insertion_point_crossing_)
    value->SetBoolean("insertionPointCrossing", true);
  if (invalidates_slotted_)
    value->SetBoolean("invalidatesSlotted", true);

  if (ids_) {
    value->BeginArray("ids");
    for (const auto& id : *ids_)
      value->PushString(id);
    value->EndArray();
  }

  if (classes_) {
    value->BeginArray("classes");
    for (const auto& class_name : *classes_)
      value->PushString(class_name);
    value->EndArray();
  }

  if (tag_names_) {
    value->BeginArray("tagNames");
    for (const auto& tag_name : *tag_names_)
      value->PushString(tag_name);
    value->EndArray();
  }

  if (attributes_) {
    value->BeginArray("attributes");
    for (const auto& attribute : *attributes_)
      value->PushString(attribute);
    value->EndArray();
  }

  value->EndDictionary();
}

#ifndef NDEBUG
void InvalidationSet::Show() const {
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  value->BeginArray("InvalidationSet");
  ToTracedValue(value.get());
  value->EndArray();
  fprintf(stderr, "%s\n", value->ToString().Ascii().data());
}
#endif  // NDEBUG

SiblingInvalidationSet::SiblingInvalidationSet(
    RefPtr<DescendantInvalidationSet> descendants)
    : InvalidationSet(kInvalidateSiblings),
      max_direct_adjacent_selectors_(1),
      descendant_invalidation_set_(std::move(descendants)) {}

DescendantInvalidationSet& SiblingInvalidationSet::EnsureSiblingDescendants() {
  if (!sibling_descendant_invalidation_set_)
    sibling_descendant_invalidation_set_ = DescendantInvalidationSet::Create();
  return *sibling_descendant_invalidation_set_;
}

DescendantInvalidationSet& SiblingInvalidationSet::EnsureDescendants() {
  if (!descendant_invalidation_set_)
    descendant_invalidation_set_ = DescendantInvalidationSet::Create();
  return *descendant_invalidation_set_;
}

}  // namespace blink
