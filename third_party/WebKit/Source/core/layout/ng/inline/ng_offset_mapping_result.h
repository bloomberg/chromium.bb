// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGOffsetMappingResult_h
#define NGOffsetMappingResult_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Node;

enum class NGOffsetMappingUnitType { kIdentity, kCollapsed, kExpanded };

// An NGOffsetMappingUnit indicates a "simple" offset mapping between dom offset
// range [dom_start, dom_end] on node |owner| and text content offset range
// [text_content_start, text_content_end]. The mapping between them falls in one
// of the following categories, depending on |type|:
// - kIdentity: The mapping between the two ranges is the identity mapping. In
//   other words, the two ranges have the same length, and the offsets are
//   mapped one-to-one.
// - kCollapsed: The mapping is collapsed, namely, |text_content_start| and
//   |text_content_end| are the same, and characters in the dom range are
//   collapsed.
// - kExpanded: The mapping is expanded, namely, |dom_end == dom_start + 1|, and
//   |text_content_end > text_content_start + 1|, indicating that the character
//   in the dom range is expanded into multiple characters.
// See design doc https://goo.gl/CJbxky for details.
class CORE_EXPORT NGOffsetMappingUnit {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  NGOffsetMappingUnit(NGOffsetMappingUnitType,
                      const Node&,
                      unsigned dom_start,
                      unsigned dom_end,
                      unsigned text_content_start,
                      unsigned text_content_end);
  ~NGOffsetMappingUnit();

  NGOffsetMappingUnitType GetType() const { return type_; }
  const Node& GetOwner() const { return *owner_; }
  unsigned DOMStart() const { return dom_start_; }
  unsigned DOMEnd() const { return dom_end_; }
  unsigned TextContentStart() const { return text_content_start_; }
  unsigned TextContentEnd() const { return text_content_end_; }

  unsigned ConvertDOMOffsetToTextContent(unsigned) const;

 private:
  const NGOffsetMappingUnitType type_ = NGOffsetMappingUnitType::kIdentity;

  const Persistent<const Node> owner_;
  const unsigned dom_start_;
  const unsigned dom_end_;
  const unsigned text_content_start_;
  const unsigned text_content_end_;
};

class NGMappingUnitRange {
  STACK_ALLOCATED();

 public:
  const NGOffsetMappingUnit* begin() const { return begin_; }
  const NGOffsetMappingUnit* end() const { return end_; }

  NGMappingUnitRange() : begin_(nullptr), end_(nullptr) {}
  NGMappingUnitRange(const NGOffsetMappingUnit* begin,
                     const NGOffsetMappingUnit* end)
      : begin_(begin), end_(end) {}

 private:
  const NGOffsetMappingUnit* begin_;
  const NGOffsetMappingUnit* end_;
};

// An NGOffsetMappingResult stores the units of a LayoutNGBlockFlow in sorted
// order in a vector. For each text node, the index range of the units owned by
// the node is also stored.
// See design doc https://goo.gl/CJbxky for details.
class CORE_EXPORT NGOffsetMappingResult {
 public:
  using UnitVector = Vector<NGOffsetMappingUnit>;
  using RangeMap =
      HashMap<Persistent<const Node>, std::pair<unsigned, unsigned>>;

  NGOffsetMappingResult(NGOffsetMappingResult&&);
  NGOffsetMappingResult(UnitVector&&, RangeMap&&, String);
  ~NGOffsetMappingResult();

  const UnitVector& GetUnits() const { return units_; }
  const RangeMap& GetRanges() const { return ranges_; }
  const String& GetText() const { return text_; }

  // Returns the NGOffsetMappingUnit that contains the given offset in the DOM
  // node. If there are multiple qualifying units, returns the last one.
  const NGOffsetMappingUnit* GetMappingUnitForDOMOffset(const Node&,
                                                        unsigned) const;

  // Returns all NGOffsetMappingUnits whose DOM ranges has non-empty (but
  // possibly collapsed) intersections with the passed in DOM offset range.
  NGMappingUnitRange GetMappingUnitsForDOMOffsetRange(const Node&,
                                                      unsigned,
                                                      unsigned) const;

  // Returns the text content offset corresponding to the given DOM offset.
  // TODO(xiaochengh): Use Optional<> return type to indicate inexistent result.
  size_t GetTextContentOffset(const Node&, unsigned) const;

  // Starting from the given DOM offset in the node, finds the first
  // non-collapsed character and returns its offset; Or returns the last offset
  // in the node if such a character does not exist.
  // TODO(xiaochengh): Use Optional<> return type to indicate inexistent result.
  unsigned StartOfNextNonCollapsedCharacter(const Node&, unsigned offset) const;

  // Starting from the given DOM offset in the node, reversely finds the first
  // non-collapsed character and returns 1 + its offset; Or returns 0 if such a
  // character does not exist.
  // TODO(xiaochengh): Use Optional<> return type to indicate inexistent result.
  unsigned EndOfLastNonCollapsedCharacter(const Node&, unsigned offset) const;

  // Returns true if the character at the position is non-collapsed. If the
  // offset is at the end of the node, returns false.
  // TODO(xiaochengh): Rename to IsBeforeNonCollapsedCharacter().
  bool IsNonCollapsedCharacter(const Node&, unsigned offset) const;

  // Returns true if the offset is right after a non-collapsed character. If the
  // offset is at the beginning of the node, returns false.
  bool IsAfterNonCollapsedCharacter(const Node&, unsigned offset) const;

  // Maps the given DOM offset to text content, and then returns the text
  // content character before the offset. Returns nullopt if it does not exist.
  Optional<UChar> GetCharacterBefore(const Node&, unsigned offset) const;

  // TODO(xiaochengh): Add APIs for reverse mapping.

 private:
  UnitVector units_;
  RangeMap ranges_;
  String text_;

  DISALLOW_COPY_AND_ASSIGN(NGOffsetMappingResult);
};

const NGOffsetMappingResult* GetNGOffsetMappingFor(const Node&, unsigned);

}  // namespace blink

#endif  // NGOffsetMappingResult_h
