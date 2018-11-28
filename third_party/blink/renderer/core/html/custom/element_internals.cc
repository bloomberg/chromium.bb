// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/element_internals.h"

#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

ElementInternals::ElementInternals(HTMLElement& target) : target_(target) {
  value_.SetUSVString(String());
}

void ElementInternals::Trace(Visitor* visitor) {
  visitor->Trace(target_);
  visitor->Trace(value_);
  visitor->Trace(entry_source_);
  ListedElement::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

void ElementInternals::setFormValue(const FileOrUSVString& value) {
  setFormValue(value, nullptr);
}

void ElementInternals::setFormValue(const FileOrUSVString& value,
                                    FormData* entry_source) {
  if (!entry_source || entry_source->size() == 0u) {
    value_ = value;
    entry_source_ = nullptr;
    return;
  }
  value_ = value;
  entry_source_ = MakeGarbageCollected<FormData>(*entry_source);
}

HTMLFormElement* ElementInternals::form() const {
  return ListedElement::Form();
}

void ElementInternals::DidUpgrade() {
  ContainerNode* parent = Target().parentNode();
  if (!parent)
    return;
  InsertedInto(*parent);
  if (auto* owner_form = form()) {
    if (auto* lists = owner_form->NodeLists())
      lists->InvalidateCaches(nullptr);
  }
  for (ContainerNode* node = parent; node; node = node->parentNode()) {
    if (IsHTMLFieldSetElement(node)) {
      // TODO(tkent): Invalidate only HTMLFormControlsCollections.
      if (auto* lists = node->NodeLists())
        lists->InvalidateCaches(nullptr);
    }
  }
}

bool ElementInternals::IsFormControlElement() const {
  return false;
}

bool ElementInternals::IsElementInternals() const {
  return true;
}

bool ElementInternals::IsEnumeratable() const {
  return true;
}

void ElementInternals::AppendToFormData(FormData& form_data) {
  const AtomicString& name = Target().FastGetAttribute(html_names::kNameAttr);
  if (!entry_source_ || entry_source_->size() == 0u) {
    if (name.IsNull())
      return;
    if (value_.IsFile())
      form_data.AppendFromElement(name, value_.GetAsFile());
    else if (value_.IsUSVString())
      form_data.AppendFromElement(name, value_.GetAsUSVString());
    else
      form_data.AppendFromElement(name, g_empty_string);
    return;
  }
  for (const auto& entry : entry_source_->Entries()) {
    if (entry->isFile())
      form_data.append(entry->name(), entry->GetFile());
    else
      form_data.append(entry->name(), entry->Value());
  }
}

void ElementInternals::DidChangeForm() {
  ListedElement::DidChangeForm();
  CustomElement::EnqueueFormAssociatedCallback(Target(), form());
}

}  // namespace blink
