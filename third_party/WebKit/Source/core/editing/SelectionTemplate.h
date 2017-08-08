// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SelectionTemplate_h
#define SelectionTemplate_h

#include <iosfwd>
#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/Position.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/editing/SelectionType.h"
#include "core/editing/TextAffinity.h"
#include "platform/wtf/Allocator.h"

namespace blink {

// SelectionTemplate is used for representing a selection in DOM tree or Flat
// tree with template parameter |Strategy|. Instances of |SelectionTemplate|
// are immutable objects, you can't change them once constructed.
//
// To construct |SelectionTemplate| object, please use |Builder| class.
template <typename Strategy>
class CORE_EXPORT SelectionTemplate final {
  DISALLOW_NEW();

 public:
  // |Builder| is a helper class for constructing |SelectionTemplate| object.
  class CORE_EXPORT Builder final {
    DISALLOW_NEW();

   public:
    explicit Builder(const SelectionTemplate&);
    Builder();

    SelectionTemplate Build() const;

    // Move selection to |base|. |base| can't be null.
    Builder& Collapse(const PositionTemplate<Strategy>& base);
    Builder& Collapse(const PositionWithAffinityTemplate<Strategy>& base);

    // Extend selection to |extent|. It is error if selection is none.
    // |extent| can be in different tree scope of base, but should be in same
    // document.
    Builder& Extend(const PositionTemplate<Strategy>& extent);

    // Select all children in |node|.
    Builder& SelectAllChildren(const Node& /* node */);

    Builder& SetBaseAndExtent(const EphemeralRangeTemplate<Strategy>&);

    // |extent| can not be null if |base| isn't null.
    Builder& SetBaseAndExtent(const PositionTemplate<Strategy>& base,
                              const PositionTemplate<Strategy>& extent);

    // |extent| can be non-null while |base| is null, which is undesired.
    // Note: This function should be used only in "core/editing/commands".
    // TODO(yosin): Once all we get rid of all call sites, we remove this.
    Builder& SetBaseAndExtentDeprecated(
        const PositionTemplate<Strategy>& base,
        const PositionTemplate<Strategy>& extent);

    Builder& SetAffinity(TextAffinity);
    Builder& SetIsDirectional(bool);

   private:
    SelectionTemplate selection_;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  SelectionTemplate(const SelectionTemplate& other);
  SelectionTemplate();

  SelectionTemplate& operator=(const SelectionTemplate&) = default;

  bool operator==(const SelectionTemplate&) const;
  bool operator!=(const SelectionTemplate&) const;

  const PositionTemplate<Strategy>& Base() const;
  const PositionTemplate<Strategy>& Extent() const;
  TextAffinity Affinity() const { return affinity_; }
  bool IsBaseFirst() const;
  bool IsCaret() const;
  bool IsDirectional() const { return is_directional_; }
  bool IsNone() const { return base_.IsNull(); }
  bool IsRange() const;

  // Returns true if |this| selection holds valid values otherwise it causes
  // assertion failure.
  bool AssertValid() const;
  bool AssertValidFor(const Document&) const;

  const PositionTemplate<Strategy>& ComputeEndPosition() const;
  const PositionTemplate<Strategy>& ComputeStartPosition() const;

  // Returns |SelectionType| for |this| based on |base_| and |extent_|.
  SelectionType Type() const;

  DECLARE_TRACE();

  void PrintTo(std::ostream*, const char* type) const;
#ifndef NDEBUG
  void ShowTreeForThis() const;
#endif

 private:
  friend class SelectionEditor;

  enum class Direction {
    kNotComputed,
    kForward,   // base <= extent
    kBackward,  // base > extent
  };

  Document* GetDocument() const;
  void ResetDirectionCache() const;

  PositionTemplate<Strategy> base_;
  PositionTemplate<Strategy> extent_;
  TextAffinity affinity_ = TextAffinity::kDownstream;
  mutable Direction direction_ = Direction::kForward;
  bool is_directional_ = false;
#if DCHECK_IS_ON()
  uint64_t dom_tree_version_;
#endif
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    SelectionTemplate<EditingStrategy>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    SelectionTemplate<EditingInFlatTreeStrategy>;

using SelectionInDOMTree = SelectionTemplate<EditingStrategy>;
using SelectionInFlatTree = SelectionTemplate<EditingInFlatTreeStrategy>;

CORE_EXPORT std::ostream& operator<<(std::ostream&, const SelectionInDOMTree&);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const SelectionInFlatTree&);

}  // namespace blink

#endif  // SelectionTemplate_h
