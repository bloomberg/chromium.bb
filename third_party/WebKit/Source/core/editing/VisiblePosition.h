/*
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VisiblePosition_h
#define VisiblePosition_h

#include "core/CoreExport.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/editing/TextAffinity.h"
#include "platform/heap/Handle.h"
#include <iosfwd>

namespace blink {

// VisiblePosition default affinity is downstream because
// the callers do not really care (they just want the
// deep position without regard to line position), and this
// is cheaper than UPSTREAM
#define VP_DEFAULT_AFFINITY TextAffinity::kDownstream

// Callers who do not know where on the line the position is,
// but would like UPSTREAM if at a line break or DOWNSTREAM
// otherwise, need a clear way to specify that.  The
// constructors auto-correct UPSTREAM to DOWNSTREAM if the
// position is not at a line break.
#define VP_UPSTREAM_IF_POSSIBLE TextAffinity::kUpstream

// |VisiblePosition| is an immutable object representing "canonical position"
// with affinity.
//
// "canonical position" is roughly equivalent to a position where we can place
// caret, see |canonicalPosition()| for actual definition.
//
// "affinity" represents a place of caret at wrapped line. UPSTREAM affinity
// means caret is placed at end of line. DOWNSTREAM affinity means caret is
// placed at start of line.
//
// Example of affinity:
//    abc^def where "^" represent |Position|
// When above text line wrapped after "abc"
//    abc|  UPSTREAM |VisiblePosition|
//    |def  DOWNSTREAM |VisiblePosition|
//
// NOTE: UPSTREAM affinity will be used only if pos is at end of a wrapped line,
// otherwise it will be converted to DOWNSTREAM.
template <typename Strategy>
class CORE_TEMPLATE_CLASS_EXPORT VisiblePositionTemplate final {
  DISALLOW_NEW();

 public:
  VisiblePositionTemplate();

  // Node: Other than |createVisiblePosition()| and
  // |createVisiblePositionDeprecated()|, we should not use |create()|.
  static VisiblePositionTemplate Create(
      const PositionWithAffinityTemplate<Strategy>&);

  // Intentionally delete |operator==()| and |operator!=()| for reducing
  // compilation error message.
  // TODO(yosin) We'll have |equals()| when we have use cases of checking
  // equality of both position and affinity.
  bool operator==(const VisiblePositionTemplate&) const = delete;
  bool operator!=(const VisiblePositionTemplate&) const = delete;

  bool IsValid() const;

  // TODO(xiaochengh): We should have |DCHECK(isValid())| in the following
  // functions. However, there are some clients storing a VisiblePosition and
  // inspecting its properties after mutation. This should be fixed.
  bool IsNull() const { return position_with_affinity_.IsNull(); }
  bool IsNotNull() const { return position_with_affinity_.IsNotNull(); }
  bool IsOrphan() const { return DeepEquivalent().IsOrphan(); }

  PositionTemplate<Strategy> DeepEquivalent() const {
    return position_with_affinity_.GetPosition();
  }
  PositionTemplate<Strategy> ToParentAnchoredPosition() const {
    return DeepEquivalent().ParentAnchoredEquivalent();
  }
  PositionWithAffinityTemplate<Strategy> ToPositionWithAffinity() const {
    return position_with_affinity_;
  }
  TextAffinity Affinity() const { return position_with_affinity_.Affinity(); }

  static VisiblePositionTemplate<Strategy> AfterNode(const Node&);
  static VisiblePositionTemplate<Strategy> BeforeNode(Node*);
  static VisiblePositionTemplate<Strategy> FirstPositionInNode(Node*);
  static VisiblePositionTemplate<Strategy> InParentAfterNode(const Node&);
  static VisiblePositionTemplate<Strategy> InParentBeforeNode(const Node&);
  static VisiblePositionTemplate<Strategy> LastPositionInNode(Node*);

  DECLARE_TRACE();

#ifndef NDEBUG
  void ShowTreeForThis() const;
#endif

 private:
  explicit VisiblePositionTemplate(
      const PositionWithAffinityTemplate<Strategy>&);

  PositionWithAffinityTemplate<Strategy> position_with_affinity_;

#if DCHECK_IS_ON()
  uint64_t dom_tree_version_;
  uint64_t style_version_;
#endif
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    VisiblePositionTemplate<EditingStrategy>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    VisiblePositionTemplate<EditingInFlatTreeStrategy>;

using VisiblePosition = VisiblePositionTemplate<EditingStrategy>;
using VisiblePositionInFlatTree =
    VisiblePositionTemplate<EditingInFlatTreeStrategy>;

CORE_EXPORT VisiblePosition
CreateVisiblePosition(const Position&, TextAffinity = VP_DEFAULT_AFFINITY);
CORE_EXPORT VisiblePosition CreateVisiblePosition(const PositionWithAffinity&);
CORE_EXPORT VisiblePositionInFlatTree
CreateVisiblePosition(const PositionInFlatTree&,
                      TextAffinity = VP_DEFAULT_AFFINITY);
CORE_EXPORT VisiblePositionInFlatTree
CreateVisiblePosition(const PositionInFlatTreeWithAffinity&);

CORE_EXPORT std::ostream& operator<<(std::ostream&, const VisiblePosition&);
CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                     const VisiblePositionInFlatTree&);

}  // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const blink::VisiblePosition*);
void showTree(const blink::VisiblePosition&);
#endif

#endif  // VisiblePosition_h
