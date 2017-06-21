/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#ifndef PseudoElement_h
#define PseudoElement_h

#include "core/CoreExport.h"
#include "core/dom/Element.h"
#include "core/style/ComputedStyle.h"

namespace blink {

class CORE_EXPORT PseudoElement : public Element {
 public:
  static PseudoElement* Create(Element* parent, PseudoId);

  PassRefPtr<ComputedStyle> CustomStyleForLayoutObject() override;
  void AttachLayoutTree(AttachContext&) override;
  bool LayoutObjectIsNeeded(const ComputedStyle&) override;

  bool CanStartSelection() const override { return false; }
  bool CanContainRangeEndPoint() const override { return false; }
  PseudoId GetPseudoId() const override { return pseudo_id_; }

  static String PseudoElementNameForEvents(PseudoId);

  Node* FindAssociatedNode() const;

  virtual void Dispose();

 protected:
  PseudoElement(Element*, PseudoId);

 private:
  void DidRecalcStyle() override;

  PseudoId pseudo_id_;
};

const QualifiedName& PseudoElementTagName();

inline bool PseudoElementLayoutObjectIsNeeded(const ComputedStyle* style) {
  if (!style)
    return false;
  if (style->Display() == EDisplay::kNone)
    return false;
  if (style->StyleType() == kPseudoIdFirstLetter ||
      style->StyleType() == kPseudoIdBackdrop)
    return true;
  return style->GetContentData();
}

DEFINE_ELEMENT_TYPE_CASTS(PseudoElement, IsPseudoElement());

}  // namespace blink

#endif
