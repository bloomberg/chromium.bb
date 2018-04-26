// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/part_names.h"

#include "third_party/blink/renderer/core/dom/names_map.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"

namespace blink {

namespace {
// Adds the names to the set.
static void AddToSet(const SpaceSplitString& strings,
                     HashSet<AtomicString>* set) {
  for (size_t i = 0; i < strings.size(); i++) {
    set->insert(strings[i]);
  }
}
}  // namespace

PartNames::PartNames(const SpaceSplitString& names) {
  AddToSet(names, &names_);
}

void PartNames::ApplyMap(const NamesMap& names_map) {
  // TODO(crbug/805271): Don't apply mappings immediately, instead, queue them
  // up and only apply them if Contains is actually called.
  HashSet<AtomicString> new_names;
  for (const AtomicString& name : names_) {
    if (base::Optional<SpaceSplitString> mapped_names = names_map.Get(name))
      AddToSet(mapped_names.value(), &new_names);
  }
  std::swap(names_, new_names);
}

bool PartNames::Contains(const AtomicString& name) {
  return names_.Contains(name);
}

size_t PartNames::size() {
  return names_.size();
}

}  // namespace blink
