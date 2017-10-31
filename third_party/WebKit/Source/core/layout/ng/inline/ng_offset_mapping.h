// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGOffsetMapping_h
#define NGOffsetMapping_h

#include "core/CoreExport.h"
#include "core/editing/Forward.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class LayoutObject;
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

  unsigned ConvertTextContentToFirstDOMOffset(unsigned) const;
  unsigned ConvertTextContentToLastDOMOffset(unsigned) const;

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

// An NGOffsetMapping stores the units of a LayoutNGBlockFlow in sorted
// order in a vector. For each text node, the index range of the units owned by
// the node is also stored.
// See design doc https://goo.gl/CJbxky for details.
class CORE_EXPORT NGOffsetMapping {
 public:
  using UnitVector = Vector<NGOffsetMappingUnit>;
  using RangeMap =
      HashMap<Persistent<const Node>, std::pair<unsigned, unsigned>>;

  NGOffsetMapping(NGOffsetMapping&&);
  NGOffsetMapping(UnitVector&&, RangeMap&&, String);
  ~NGOffsetMapping();

  const UnitVector& GetUnits() const { return units_; }
  const RangeMap& GetRanges() const { return ranges_; }
  const String& GetText() const { return text_; }

  // ------ Mapping APIs from DOM to text content ------

  // TODO(xiaochengh): Change the functions to take Positions instead of (node,
  // offset) pairs.

  // Note: any Position passed to the APIs must be in either of the two types:
  // 1. Offset-in-anchor in text node
  // 2. Before/After-anchor of BR or atomic inline
  // TODO(crbug.com/776843): support non-atomic inlines
  static bool AcceptsPosition(const Position&);

  // Returns the mapping object of the block laying out the given position.
  static const NGOffsetMapping* GetFor(const Position&);

  // Returns the mapping object of the block containing the given legacy
  // LayoutObject, if it's laid out with NG. This makes the retrieval of the
  // mapping object easier when we are holding LayoutObject instead of Node.
  static const NGOffsetMapping* GetFor(const LayoutObject*);

  // Returns the NGOffsetMappingUnit that contains the given offset in the DOM
  // node. If there are multiple qualifying units, returns the last one.
  const NGOffsetMappingUnit* GetMappingUnitForDOMOffset(const Node&,
                                                        unsigned) const;

  // Returns all NGOffsetMappingUnits whose DOM ranges has non-empty (but
  // possibly collapsed) intersections with the passed in DOM offset range.
  NGMappingUnitRange GetMappingUnitsForDOMOffsetRange(const Node&,
                                                      unsigned,
                                                      unsigned) const;

  // Returns the text content offset corresponding to the given position.
  // Returns nullopt when the position is not laid out in this block.
  Optional<unsigned> GetTextContentOffset(const Position&) const;

  // Starting from the given position, searches for non-collapsed content in
  // the same node in forward/backward direction and returns the position
  // before/after it; Returns null if there is no more non-collapsed content in
  // the same node.
  Position StartOfNextNonCollapsedContent(const Position&) const;
  Position EndOfLastNonCollapsedContent(const Position&) const;

  // Returns true if the offset is right before a non-collapsed character. If
  // the offset is at the end of the node, returns false.
  bool IsBeforeNonCollapsedCharacter(const Node&, unsigned offset) const;

  // Returns true if the offset is right after a non-collapsed character. If the
  // offset is at the beginning of the node, returns false.
  bool IsAfterNonCollapsedCharacter(const Node&, unsigned offset) const;

  // Maps the given DOM offset to text content, and then returns the text
  // content character before the offset. Returns nullopt if it does not exist.
  Optional<UChar> GetCharacterBefore(const Node&, unsigned offset) const;

  // ------ Mapping APIs from text content to DOM ------

  // These APIs map a text content offset to DOM positions, or return null when
  // both characters next to the offset are in generated content (list markers,
  // ::before/after, generated BiDi control characters, ...). The returned
  // position is either offset in a text node, or before/after an atomic inline
  // (IMG, BR, ...).
  // Note 1: there can be multiple positions mapped to the same offset when,
  // for example, there are collapsed whitespaces. Hence, we have two APIs to
  // return the first/last one of them.
  // Note 2: there is a corner case where Shadow DOM changes the ordering of
  // nodes in the flat tree, so that they are not laid out in the same order as
  // in the DOM tree. In this case, "first" and "last" position are defined on
  // the layout order, aka the flat tree order.
  Position GetFirstPosition(unsigned) const;
  Position GetLastPosition(unsigned) const;

 private:
  UnitVector units_;
  RangeMap ranges_;
  String text_;

  DISALLOW_COPY_AND_ASSIGN(NGOffsetMapping);
};

}  // namespace blink

#endif  // NGOffsetMapping_h
