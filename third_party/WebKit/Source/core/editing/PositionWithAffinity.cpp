// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file./*

#include "core/editing/PositionWithAffinity.h"

namespace blink {

template <typename Strategy>
PositionWithAffinityTemplate<Strategy>::PositionWithAffinityTemplate(
    const PositionTemplate<Strategy>& position,
    TextAffinity affinity)
    : position_(position), affinity_(affinity) {}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy>::PositionWithAffinityTemplate()
    : affinity_(TextAffinity::kDownstream) {}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy>::~PositionWithAffinityTemplate() {}

template <typename Strategy>
void PositionWithAffinityTemplate<Strategy>::Trace(blink::Visitor* visitor) {
  visitor->Trace(position_);
}

template <typename Strategy>
bool PositionWithAffinityTemplate<Strategy>::operator==(
    const PositionWithAffinityTemplate& other) const {
  if (IsNull())
    return other.IsNull();
  return affinity_ == other.affinity_ && position_ == other.position_;
}

template class CORE_TEMPLATE_EXPORT
    PositionWithAffinityTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    PositionWithAffinityTemplate<EditingInFlatTreeStrategy>;

std::ostream& operator<<(std::ostream& ostream,
                         const PositionWithAffinity& position_with_affinity) {
  return ostream << position_with_affinity.GetPosition() << '/'
                 << position_with_affinity.Affinity();
}

std::ostream& operator<<(
    std::ostream& ostream,
    const PositionInFlatTreeWithAffinity& position_with_affinity) {
  return ostream << position_with_affinity.GetPosition() << '/'
                 << position_with_affinity.Affinity();
}

}  // namespace blink
