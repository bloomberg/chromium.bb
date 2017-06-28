/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef HTMLContentElement_h
#define HTMLContentElement_h

#include "core/CoreExport.h"
#include "core/css/CSSSelectorList.h"
#include "core/dom/InsertionPoint.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLContentSelectFilter
    : public GarbageCollectedFinalized<HTMLContentSelectFilter> {
 public:
  virtual ~HTMLContentSelectFilter() {}
  virtual bool CanSelectNode(const HeapVector<Member<Node>, 32>& siblings,
                             int nth) const = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

class CORE_EXPORT HTMLContentElement final : public InsertionPoint {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static HTMLContentElement* Create(Document&,
                                    HTMLContentSelectFilter* = nullptr);
  ~HTMLContentElement() override;

  bool CanAffectSelector() const override { return true; }

  bool CanSelectNode(const HeapVector<Member<Node>, 32>& siblings,
                     int nth) const;

  const CSSSelectorList& SelectorList() const;
  bool IsSelectValid() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  HTMLContentElement(Document&, HTMLContentSelectFilter*);

  void ParseAttribute(const AttributeModificationParams&) override;

  bool ValidateSelect() const;
  void ParseSelect();

  bool MatchSelector(Element&) const;

  bool should_parse_select_;
  bool is_valid_selector_;
  AtomicString select_;
  CSSSelectorList selector_list_;
  Member<HTMLContentSelectFilter> filter_;
};

inline const CSSSelectorList& HTMLContentElement::SelectorList() const {
  if (should_parse_select_)
    const_cast<HTMLContentElement*>(this)->ParseSelect();
  return selector_list_;
}

inline bool HTMLContentElement::IsSelectValid() const {
  if (should_parse_select_)
    const_cast<HTMLContentElement*>(this)->ParseSelect();
  return is_valid_selector_;
}

inline bool HTMLContentElement::CanSelectNode(
    const HeapVector<Member<Node>, 32>& siblings,
    int nth) const {
  if (filter_)
    return filter_->CanSelectNode(siblings, nth);
  if (select_.IsNull() || select_.IsEmpty())
    return true;
  if (!IsSelectValid())
    return false;
  if (!siblings[nth]->IsElementNode())
    return false;
  return MatchSelector(*ToElement(siblings[nth]));
}

}  // namespace blink

#endif  // HTMLContentElement_h
