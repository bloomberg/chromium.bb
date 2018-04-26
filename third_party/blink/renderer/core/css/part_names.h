// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PART_NAMES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PART_NAMES_H_

#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class NamesMap;
class SpaceSplitString;

// Represents a set of part names.
class PartNames {
 public:
  PartNames();
  explicit PartNames(const SpaceSplitString& names);
  // Updates the set, using names_map to map all of the current names to a new
  // set of names. Looks up each name in names_map and takes the union of all
  // of the values (which are sets of names).
  void ApplyMap(const NamesMap& names_map);
  // Returns true of name is included in the set.
  bool Contains(const AtomicString& name);
  // Returns the number of part names in the set.
  size_t size();

 private:
  HashSet<AtomicString> names_;

  DISALLOW_COPY_AND_ASSIGN(PartNames);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PART_NAMES_H_
