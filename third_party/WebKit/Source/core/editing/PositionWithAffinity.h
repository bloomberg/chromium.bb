// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file./*

#ifndef PositionWithAffinity_h
#define PositionWithAffinity_h

#include "core/CoreExport.h"
#include "core/editing/Position.h"
#include "core/editing/TextAffinity.h"
#include <iosfwd>

namespace blink {

template <typename Strategy>
class CORE_TEMPLATE_CLASS_EXPORT PositionWithAffinityTemplate {
  DISALLOW_NEW();

 public:
  // TODO(yosin) We should have single parameter constructor not to use
  // default parameter for avoiding include "TextAffinity.h"
  PositionWithAffinityTemplate(const PositionTemplate<Strategy>&,
                               TextAffinity = TextAffinity::kDownstream);
  PositionWithAffinityTemplate();
  ~PositionWithAffinityTemplate();

  TextAffinity Affinity() const { return affinity_; }
  const PositionTemplate<Strategy>& GetPosition() const { return position_; }

  // Returns true if both |this| and |other| is null or both |m_position|
  // and |m_affinity| equal.
  bool operator==(const PositionWithAffinityTemplate& other) const;
  bool operator!=(const PositionWithAffinityTemplate& other) const {
    return !operator==(other);
  }

  bool IsValidFor(const Document& document) const {
    return position_.IsValidFor(document);
  }

  bool IsNotNull() const { return position_.IsNotNull(); }
  bool IsNull() const { return position_.IsNull(); }
  bool IsOrphan() const { return position_.IsOrphan(); }
  bool IsConnected() const { return position_.IsConnected(); }

  Node* AnchorNode() const { return position_.AnchorNode(); }
  Document* GetDocument() const { return position_.GetDocument(); }

  void Trace(blink::Visitor*);

 private:
  PositionTemplate<Strategy> position_;
  TextAffinity affinity_;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    PositionWithAffinityTemplate<EditingStrategy>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    PositionWithAffinityTemplate<EditingInFlatTreeStrategy>;

using PositionWithAffinity = PositionWithAffinityTemplate<EditingStrategy>;
using PositionInFlatTreeWithAffinity =
    PositionWithAffinityTemplate<EditingInFlatTreeStrategy>;

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> FromPositionInDOMTree(
    const PositionWithAffinity&);

template <>
inline PositionWithAffinity FromPositionInDOMTree<EditingStrategy>(
    const PositionWithAffinity& position_with_affinity) {
  return position_with_affinity;
}

template <>
inline PositionInFlatTreeWithAffinity
FromPositionInDOMTree<EditingInFlatTreeStrategy>(
    const PositionWithAffinity& position_with_affinity) {
  return PositionInFlatTreeWithAffinity(
      ToPositionInFlatTree(position_with_affinity.GetPosition()),
      position_with_affinity.Affinity());
}

CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                     const PositionWithAffinity&);
CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                     const PositionInFlatTreeWithAffinity&);

}  // namespace blink

#endif  // PositionWithAffinity_h
