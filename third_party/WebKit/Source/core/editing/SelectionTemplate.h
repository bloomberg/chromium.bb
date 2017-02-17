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
#include "core/editing/TextGranularity.h"
#include "wtf/Allocator.h"

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

    SelectionTemplate build() const;

    // Move selection to |base|. |base| can't be null.
    Builder& collapse(const PositionTemplate<Strategy>& base);
    Builder& collapse(const PositionWithAffinityTemplate<Strategy>& base);

    // Extend selection to |extent|. It is error if selection is none.
    // |extent| can be in different tree scope of base, but should be in same
    // document.
    Builder& extend(const PositionTemplate<Strategy>& extent);

    // Select all children in |node|.
    Builder& selectAllChildren(const Node& /* node */);

    Builder& setBaseAndExtent(const EphemeralRangeTemplate<Strategy>&);

    // |extent| can not be null if |base| isn't null.
    Builder& setBaseAndExtent(const PositionTemplate<Strategy>& base,
                              const PositionTemplate<Strategy>& extent);

    // |extent| can be non-null while |base| is null, which is undesired.
    // Note: This function should be used only in "core/editing/commands".
    // TODO(yosin): Once all we get rid of all call sites, we remove this.
    Builder& setBaseAndExtentDeprecated(
        const PositionTemplate<Strategy>& base,
        const PositionTemplate<Strategy>& extent);

    Builder& setAffinity(TextAffinity);
    Builder& setGranularity(TextGranularity);
    Builder& setHasTrailingWhitespace(bool);
    Builder& setIsDirectional(bool);
    Builder& setIsHandleVisible(bool);

   private:
    SelectionTemplate m_selection;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  SelectionTemplate(const SelectionTemplate& other);
  SelectionTemplate();

  SelectionTemplate& operator=(const SelectionTemplate&) = default;

  bool operator==(const SelectionTemplate&) const;
  bool operator!=(const SelectionTemplate&) const;

  const PositionTemplate<Strategy>& base() const;
  const PositionTemplate<Strategy>& extent() const;
  TextAffinity affinity() const { return m_affinity; }
  TextGranularity granularity() const { return m_granularity; }
  bool hasTrailingWhitespace() const { return m_hasTrailingWhitespace; }
  bool isCaret() const { return m_base.isNotNull() && m_base == m_extent; }
  bool isDirectional() const { return m_isDirectional; }
  bool isHandleVisible() const { return m_isHandleVisible; }
  bool isNone() const { return m_base.isNull(); }
  bool isRange() const { return m_base != m_extent; }

  // Returns true if |this| selection holds valid values otherwise it causes
  // assertion failure.
  bool assertValid() const;
  bool assertValidFor(const Document&) const;

  const PositionTemplate<Strategy>& computeEndPosition() const;
  const PositionTemplate<Strategy>& computeStartPosition() const;

  // Returns |SelectionType| for |this| based on |m_base| and |m_extent|
  // If |m_granularity| is |CharacterGranularity|, otherwise this function
  // returns |RangeSelection| event if |m_base| == |m_extent|.
  // Note: |m_granularity| will be removed, using this function is not
  // encouraged.
  SelectionType selectionTypeWithLegacyGranularity() const;

  DECLARE_TRACE();

  void printTo(std::ostream*, const char* type) const;
#ifndef NDEBUG
  void showTreeForThis() const;
#endif

 private:
  friend class SelectionEditor;

  Document* document() const;

  PositionTemplate<Strategy> m_base;
  PositionTemplate<Strategy> m_extent;
  TextAffinity m_affinity = TextAffinity::Downstream;
  TextGranularity m_granularity = CharacterGranularity;
  bool m_hasTrailingWhitespace = false;
  bool m_isDirectional = false;
  bool m_isHandleVisible = false;
#if DCHECK_IS_ON()
  uint64_t m_domTreeVersion;
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
