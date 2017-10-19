// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PseudoElementData_h
#define PseudoElementData_h

#include "platform/heap/Handle.h"

namespace blink {

class PseudoElementData final : public GarbageCollected<PseudoElementData> {
  WTF_MAKE_NONCOPYABLE(PseudoElementData);

 public:
  static PseudoElementData* Create();
  void SetPseudoElement(PseudoId, PseudoElement*);
  PseudoElement* GetPseudoElement(PseudoId) const;
  bool HasPseudoElements() const;
  void ClearPseudoElements();
  void Trace(blink::Visitor* visitor) {
    visitor->Trace(generated_before_);
    visitor->Trace(generated_after_);
    visitor->Trace(generated_first_letter_);
    visitor->Trace(backdrop_);
  }

 private:
  PseudoElementData() = default;
  Member<PseudoElement> generated_before_;
  Member<PseudoElement> generated_after_;
  Member<PseudoElement> generated_first_letter_;
  Member<PseudoElement> backdrop_;
};

inline PseudoElementData* PseudoElementData::Create() {
  return new PseudoElementData();
}

inline bool PseudoElementData::HasPseudoElements() const {
  return generated_before_ || generated_after_ || backdrop_ ||
         generated_first_letter_;
}

inline void PseudoElementData::ClearPseudoElements() {
  SetPseudoElement(kPseudoIdBefore, nullptr);
  SetPseudoElement(kPseudoIdAfter, nullptr);
  SetPseudoElement(kPseudoIdBackdrop, nullptr);
  SetPseudoElement(kPseudoIdFirstLetter, nullptr);
}

inline void PseudoElementData::SetPseudoElement(PseudoId pseudo_id,
                                                PseudoElement* element) {
  switch (pseudo_id) {
    case kPseudoIdBefore:
      if (generated_before_)
        generated_before_->Dispose();
      generated_before_ = element;
      break;
    case kPseudoIdAfter:
      if (generated_after_)
        generated_after_->Dispose();
      generated_after_ = element;
      break;
    case kPseudoIdBackdrop:
      if (backdrop_)
        backdrop_->Dispose();
      backdrop_ = element;
      break;
    case kPseudoIdFirstLetter:
      if (generated_first_letter_)
        generated_first_letter_->Dispose();
      generated_first_letter_ = element;
      break;
    default:
      NOTREACHED();
  }
}

inline PseudoElement* PseudoElementData::GetPseudoElement(
    PseudoId pseudo_id) const {
  switch (pseudo_id) {
    case kPseudoIdBefore:
      return generated_before_;
    case kPseudoIdAfter:
      return generated_after_;
    case kPseudoIdBackdrop:
      return backdrop_;
    case kPseudoIdFirstLetter:
      return generated_first_letter_;
    default:
      return nullptr;
  }
}

}  // namespace blink

#endif  // PseudoElementData_h
