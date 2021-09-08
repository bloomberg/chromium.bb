// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_NODE_H_
#define UI_ACCESSIBILITY_AX_NODE_H_

#include <stdint.h>

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/strings/char_traits.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_hypertext.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {

class AXTableInfo;
struct AXLanguageInfo;
struct AXTreeData;

// This class is used to represent a node in an accessibility tree (`AXTree`).
class AX_EXPORT AXNode final {
 public:
  // Replacement character used to represent an embedded (or, additionally for
  // text navigation, an empty) object. Encoded in UTF16 format. Part of the
  // Unicode Standard.
  //
  // On some platforms, most objects are represented in the text of their
  // parents with a special "embedded object character" and not with their
  // actual text contents. Also on the same platforms, if a node has only
  // ignored descendants, i.e., it appears to be empty to assistive software, we
  // need to treat it as a character and a word boundary.
  static constexpr char16_t kEmbeddedCharacter[] = u"\uFFFC";
  // We compute the embedded character's length instead of manually typing it in
  // order to avoid the two variables getting out of sync in a future update.
  static constexpr int kEmbeddedCharacterLength =
      base::CharTraits<char16_t>::length(kEmbeddedCharacter);

  // Interface to the tree class that owns an AXNode. We use this instead
  // of letting AXNode have a pointer to its AXTree directly so that we're
  // forced to think twice before calling an AXTree interface that might not
  // be necessary.
  class OwnerTree {
   public:
    struct Selection {
      bool is_backward;
      AXNodeID anchor_object_id;
      int anchor_offset;
      ax::mojom::TextAffinity anchor_affinity;
      AXNodeID focus_object_id;
      int focus_offset;
      ax::mojom::TextAffinity focus_affinity;
    };

    // See AXTree::GetAXTreeID.
    virtual AXTreeID GetAXTreeID() const = 0;
    // See AXTree::GetTableInfo.
    virtual AXTableInfo* GetTableInfo(const AXNode* table_node) const = 0;
    // See AXTree::GetFromId.
    virtual AXNode* GetFromId(AXNodeID id) const = 0;
    // See AXTree::data.
    virtual const AXTreeData& data() const = 0;

    virtual absl::optional<int> GetPosInSet(const AXNode& node) = 0;
    virtual absl::optional<int> GetSetSize(const AXNode& node) = 0;

    virtual Selection GetUnignoredSelection() const = 0;
    virtual bool GetTreeUpdateInProgressState() const = 0;
    virtual bool HasPaginationSupport() const = 0;
  };

  template <typename NodeType,
            NodeType* (NodeType::*NextSibling)() const,
            NodeType* (NodeType::*PreviousSibling)() const,
            NodeType* (NodeType::*FirstChild)() const,
            NodeType* (NodeType::*LastChild)() const>
  class ChildIteratorBase {
   public:
    ChildIteratorBase(const NodeType* parent, NodeType* child);
    ChildIteratorBase(const ChildIteratorBase& it);
    ~ChildIteratorBase() {}
    bool operator==(const ChildIteratorBase& rhs) const;
    bool operator!=(const ChildIteratorBase& rhs) const;
    ChildIteratorBase& operator++();
    ChildIteratorBase& operator--();
    NodeType* get() const;
    NodeType& operator*() const;
    NodeType* operator->() const;

   protected:
    const NodeType* parent_;
    NodeType* child_;
  };

  // The constructor requires a parent, id, and index in parent, but
  // the data is not required. After initialization, only index_in_parent
  // and unignored_index_in_parent is allowed to change, the others are
  // guaranteed to never change.
  AXNode(OwnerTree* tree,
         AXNode* parent,
         AXNodeID id,
         size_t index_in_parent,
         size_t unignored_index_in_parent = 0);
  virtual ~AXNode();

  // Accessors.
  OwnerTree* tree() const { return tree_; }
  AXNodeID id() const { return data_.id; }
  AXNode* parent() const { return parent_; }
  const AXNodeData& data() const { return data_; }
  const std::vector<AXNode*>& children() const { return children_; }
  size_t index_in_parent() const { return index_in_parent_; }

  // Returns ownership of |data_| to the caller; effectively clearing |data_|.
  AXNodeData&& TakeData();

  //
  // Methods for walking the tree.
  //

  size_t GetChildCount() const;
  // TODO(nektar): Update `BrowserAccessibility` and remove this method.
  size_t GetChildCountCrossingTreeBoundary() const;
  size_t GetUnignoredChildCount() const;
  // TODO(nektar): Update `BrowserAccessibility` and remove this method.
  size_t GetUnignoredChildCountCrossingTreeBoundary() const;
  AXNode* GetChildAt(size_t index) const;
  // TODO(nektar): Update `BrowserAccessibility` and remove this method.
  AXNode* GetChildAtCrossingTreeBoundary(size_t index) const;
  AXNode* GetUnignoredChildAtIndex(size_t index) const;
  // TODO(nektar): Update `BrowserAccessibility` and remove this method.
  AXNode* GetUnignoredChildAtIndexCrossingTreeBoundary(size_t index) const;
  AXNode* GetParent() const;
  // TODO(nektar): Update `BrowserAccessibility` and remove this method.
  AXNode* GetParentCrossingTreeBoundary() const;
  AXNode* GetUnignoredParent() const;
  // TODO(nektar): Update `BrowserAccessibility` and remove this method.
  AXNode* GetUnignoredParentCrossingTreeBoundary() const;
  size_t GetIndexInParent() const;
  size_t GetUnignoredIndexInParent() const;
  AXNode* GetFirstUnignoredChild() const;
  AXNode* GetLastUnignoredChild() const;
  AXNode* GetDeepestFirstUnignoredChild() const;
  AXNode* GetDeepestLastUnignoredChild() const;
  AXNode* GetNextUnignoredSibling() const;
  AXNode* GetPreviousUnignoredSibling() const;
  AXNode* GetNextUnignoredInTreeOrder() const;
  AXNode* GetPreviousUnignoredInTreeOrder() const;

  using UnignoredChildIterator =
      ChildIteratorBase<AXNode,
                        &AXNode::GetNextUnignoredSibling,
                        &AXNode::GetPreviousUnignoredSibling,
                        &AXNode::GetFirstUnignoredChild,
                        &AXNode::GetLastUnignoredChild>;
  UnignoredChildIterator UnignoredChildrenBegin() const;
  UnignoredChildIterator UnignoredChildrenEnd() const;

  // Walking the tree including both ignored and unignored nodes.
  // These methods consider only the direct children or siblings of a node.
  AXNode* GetFirstChild() const;
  AXNode* GetLastChild() const;
  AXNode* GetPreviousSibling() const;
  AXNode* GetNextSibling() const;

  // Returns true if the node has any of the text related roles, including
  // kStaticText, kInlineTextBox and kListMarker (for Legacy Layout). Does not
  // include any text field roles.
  bool IsText() const;

  // Returns true if the node has any line break related roles or is the child
  // of a node with line break related roles.
  bool IsLineBreak() const;

  // Set the node's accessibility data. This may be done during initialization
  // or later when the node data changes.
  void SetData(const AXNodeData& src);

  // Update this node's location. This is separate from |SetData| just because
  // changing only the location is common and should be more efficient than
  // re-copying all of the data.
  //
  // The node's location is stored as a relative bounding box, the ID of
  // the element it's relative to, and an optional transformation matrix.
  // See ax_node_data.h for details.
  void SetLocation(AXNodeID offset_container_id,
                   const gfx::RectF& location,
                   gfx::Transform* transform);

  // Set the index in parent, for example if siblings were inserted or deleted.
  void SetIndexInParent(size_t index_in_parent);

  // Update the unignored index in parent for unignored children.
  void UpdateUnignoredCachedValues();

  // Swap the internal children vector with |children|. This instance
  // now owns all of the passed children.
  void SwapChildren(std::vector<AXNode*>* children);

  // This is called when the AXTree no longer includes this node in the
  // tree. Reference counting is used on some platforms because the
  // operating system may hold onto a reference to an AXNode
  // object even after we're through with it, so this may decrement the
  // reference count and clear out the object's data.
  void Destroy();

  // Returns true if this node is equal to or a descendant of |ancestor|.
  bool IsDescendantOf(const AXNode* ancestor) const;

  // Gets the text offsets where new lines start either from the node's data or
  // by computing them and caching the result.
  std::vector<int> GetOrComputeLineStartOffsets();

  // If the color is transparent, blends with the ancestor's color.
  // Note that this is imperfect; it won't work if a node is absolute-
  // positioned outside of its ancestor. However, it handles the most
  // common cases.
  SkColor ComputeColor() const;
  SkColor ComputeBackgroundColor() const;

  // Accessing accessibility attributes.
  // See |AXNodeData| for more information.

  bool HasBoolAttribute(ax::mojom::BoolAttribute attribute) const {
    return data().HasBoolAttribute(attribute);
  }
  bool GetBoolAttribute(ax::mojom::BoolAttribute attribute) const {
    return data().GetBoolAttribute(attribute);
  }
  bool GetBoolAttribute(ax::mojom::BoolAttribute attribute, bool* value) const {
    return data().GetBoolAttribute(attribute, value);
  }

  bool HasFloatAttribute(ax::mojom::FloatAttribute attribute) const {
    return data().HasFloatAttribute(attribute);
  }
  float GetFloatAttribute(ax::mojom::FloatAttribute attribute) const {
    return data().GetFloatAttribute(attribute);
  }
  bool GetFloatAttribute(ax::mojom::FloatAttribute attribute,
                         float* value) const {
    return data().GetFloatAttribute(attribute, value);
  }

  bool HasIntAttribute(ax::mojom::IntAttribute attribute) const {
    return data().HasIntAttribute(attribute);
  }
  int GetIntAttribute(ax::mojom::IntAttribute attribute) const {
    return data().GetIntAttribute(attribute);
  }
  bool GetIntAttribute(ax::mojom::IntAttribute attribute, int* value) const {
    return data().GetIntAttribute(attribute, value);
  }

  bool HasStringAttribute(ax::mojom::StringAttribute attribute) const {
    return data().HasStringAttribute(attribute);
  }
  const std::string& GetStringAttribute(
      ax::mojom::StringAttribute attribute) const {
    return data().GetStringAttribute(attribute);
  }
  bool GetStringAttribute(ax::mojom::StringAttribute attribute,
                          std::string* value) const {
    return data().GetStringAttribute(attribute, value);
  }

  bool GetString16Attribute(ax::mojom::StringAttribute attribute,
                            std::u16string* value) const {
    return data().GetString16Attribute(attribute, value);
  }
  std::u16string GetString16Attribute(
      ax::mojom::StringAttribute attribute) const {
    return data().GetString16Attribute(attribute);
  }

  bool HasIntListAttribute(ax::mojom::IntListAttribute attribute) const {
    return data().HasIntListAttribute(attribute);
  }
  const std::vector<int32_t>& GetIntListAttribute(
      ax::mojom::IntListAttribute attribute) const {
    return data().GetIntListAttribute(attribute);
  }
  bool GetIntListAttribute(ax::mojom::IntListAttribute attribute,
                           std::vector<int32_t>* value) const {
    return data().GetIntListAttribute(attribute, value);
  }

  bool HasStringListAttribute(ax::mojom::StringListAttribute attribute) const {
    return data().HasStringListAttribute(attribute);
  }
  const std::vector<std::string>& GetStringListAttribute(
      ax::mojom::StringListAttribute attribute) const {
    return data().GetStringListAttribute(attribute);
  }
  bool GetStringListAttribute(ax::mojom::StringListAttribute attribute,
                              std::vector<std::string>* value) const {
    return data().GetStringListAttribute(attribute, value);
  }

  bool GetHtmlAttribute(const char* attribute, std::u16string* value) const {
    return data().GetHtmlAttribute(attribute, value);
  }
  bool GetHtmlAttribute(const char* attribute, std::string* value) const {
    return data().GetHtmlAttribute(attribute, value);
  }

  // Return the hierarchical level if supported.
  absl::optional<int> GetHierarchicalLevel() const;

  // PosInSet and SetSize public methods.
  bool IsOrderedSetItem() const;
  bool IsOrderedSet() const;
  absl::optional<int> GetPosInSet();
  absl::optional<int> GetSetSize();

  // Helpers for GetPosInSet and GetSetSize.
  // Returns true if the role of ordered set matches the role of item.
  // Returns false otherwise.
  bool SetRoleMatchesItemRole(const AXNode* ordered_set) const;

  // Container objects that should be ignored for computing PosInSet and SetSize
  // for ordered sets.
  bool IsIgnoredContainerForOrderedSet() const;

  const std::string& GetInheritedStringAttribute(
      ax::mojom::StringAttribute attribute) const;
  std::u16string GetInheritedString16Attribute(
      ax::mojom::StringAttribute attribute) const;

  // If this node is a leaf, returns the inner text of this node. This is
  // equivalent to its visible accessible name. Otherwise, if this node is not a
  // leaf, represents every non-textual child node with a special "embedded
  // object character", and every textual child node with its inner text.
  // Textual nodes include e.g. static text and white space.
  //
  // This is how displayed text and embedded objects are represented in
  // ATK and IAccessible2 APIs.
  //
  // TODO(nektar): Consider changing the return value to std::string.
  std::u16string GetHypertext() const;

  // Temporary method that marks `hypertext_` dirty. This will eventually be
  // handled by the AX tree in a followup patch.
  void SetNeedsToUpdateHypertext();
  // Temporary accessor methods until hypertext is fully migrated to this class.
  // Hypertext won't eventually need to be accessed outside this class.
  const std::map<int, int>& GetHypertextOffsetToHyperlinkChildIndex() const;
  const AXHypertext& GetOldHypertext() const;

  // Returns the text that is found inside this node and all its descendants;
  // including text found in embedded objects.
  //
  // Only text displayed on screen is included. Text from ARIA and HTML
  // attributes that is either not displayed on screen, or outside this node, is
  // not returned.
  std::string GetInnerText() const;

  // Returns the length of the text (in UTF16 code units) that is found inside
  // this node and all its descendants; including text found in embedded
  // objects.
  //
  // Only text displayed on screen is counted. Text from ARIA and HTML
  // attributes that is either not displayed on screen, or outside this node, is
  // not included.
  //
  // The length of the text is in UTF8 code units, not in grapheme clusters.
  int GetInnerTextLength() const;

  // Returns a string representing the language code.
  //
  // This will consider the language declared in the DOM, and may eventually
  // attempt to automatically detect the language from the text.
  //
  // This language code will be BCP 47.
  //
  // Returns empty string if no appropriate language was found.
  std::string GetLanguage() const;

  // Returns the value of a control such as an atomic text field (<input> or
  // <textarea>), a content editable, a submit button, a slider, a progress bar,
  // a scroll bar, a meter, a spinner, a <select> element, a date picker or an
  // ARIA combo box. In order to minimize cross-process communication between
  // the renderer and the browser, this method may compute the value from the
  // control's inner text in the case of a content editable. For range controls,
  // such as sliders and scroll bars, the value of aria-valuetext takes priority
  // over the value of aria-valuenow.
  std::string GetValueForControl() const;

  //
  // Helper functions for tables, table rows, and table cells.
  // Most of these functions construct and cache an AXTableInfo behind
  // the scenes to infer many properties of tables.
  //
  // These interfaces use attributes provided by the source of the
  // AX tree where possible, but fills in missing details and ignores
  // specified attributes when they're bad or inconsistent. That way
  // you're guaranteed to get a valid, consistent table when using these
  // interfaces.
  //

  // Table-like nodes (including grids). All indices are 0-based except
  // ARIA indices are all 1-based. In other words, the top-left corner
  // of the table is row 0, column 0, cell index 0 - but that same cell
  // has a minimum ARIA row index of 1 and column index of 1.
  //
  // The below methods return absl::nullopt if the AXNode they are called on is
  // not inside a table.
  bool IsTable() const;
  absl::optional<int> GetTableColCount() const;
  absl::optional<int> GetTableRowCount() const;
  absl::optional<int> GetTableAriaColCount() const;
  absl::optional<int> GetTableAriaRowCount() const;
  absl::optional<int> GetTableCellCount() const;
  absl::optional<bool> GetTableHasColumnOrRowHeaderNode() const;
  AXNode* GetTableCaption() const;
  AXNode* GetTableCellFromIndex(int index) const;
  AXNode* GetTableCellFromCoords(int row_index, int col_index) const;
  // Get all the column header node ids of the table.
  std::vector<AXNodeID> GetTableColHeaderNodeIds() const;
  // Get the column header node ids associated with |col_index|.
  std::vector<AXNodeID> GetTableColHeaderNodeIds(int col_index) const;
  // Get the row header node ids associated with |row_index|.
  std::vector<AXNodeID> GetTableRowHeaderNodeIds(int row_index) const;
  std::vector<AXNodeID> GetTableUniqueCellIds() const;
  // Extra computed nodes for the accessibility tree for macOS:
  // one column node for each table column, followed by one
  // table header container node, or nullptr if not applicable.
  const std::vector<AXNode*>* GetExtraMacNodes() const;

  // Table row-like nodes.
  bool IsTableRow() const;
  absl::optional<int> GetTableRowRowIndex() const;
  // Get the node ids that represent rows in a table.
  std::vector<AXNodeID> GetTableRowNodeIds() const;

#if defined(OS_APPLE)
  // Table column-like nodes. These nodes are only present on macOS.
  bool IsTableColumn() const;
  absl::optional<int> GetTableColColIndex() const;
#endif  // defined(OS_APPLE)

  // Table cell-like nodes.
  bool IsTableCellOrHeader() const;
  absl::optional<int> GetTableCellIndex() const;
  absl::optional<int> GetTableCellColIndex() const;
  absl::optional<int> GetTableCellRowIndex() const;
  absl::optional<int> GetTableCellColSpan() const;
  absl::optional<int> GetTableCellRowSpan() const;
  absl::optional<int> GetTableCellAriaColIndex() const;
  absl::optional<int> GetTableCellAriaRowIndex() const;
  std::vector<AXNodeID> GetTableCellColHeaderNodeIds() const;
  std::vector<AXNodeID> GetTableCellRowHeaderNodeIds() const;
  void GetTableCellColHeaders(std::vector<AXNode*>* col_headers) const;
  void GetTableCellRowHeaders(std::vector<AXNode*>* row_headers) const;

  // Helper methods to check if a cell is an ARIA-1.1+ 'cell' or 'gridcell'
  bool IsCellOrHeaderOfARIATable() const;
  bool IsCellOrHeaderOfARIAGrid() const;

  // Return an object containing information about the languages detected on
  // this node.
  // Callers should not retain this pointer, instead they should request it
  // every time it is needed.
  //
  // Returns nullptr if the node has no language info.
  AXLanguageInfo* GetLanguageInfo() const;

  // This should only be called by LabelLanguageForSubtree and is used as part
  // of the language detection feature.
  void SetLanguageInfo(std::unique_ptr<AXLanguageInfo> lang_info);

  // Destroy the language info for this node.
  void ClearLanguageInfo();

  // Returns true if node is a group and is a direct descendant of a set-like
  // element.
  bool IsEmbeddedGroup() const;

  // Returns true if node has ignored state or ignored role.
  bool IsIgnored() const;

  // Some nodes are not ignored but should be skipped during text navigation.
  // For example, on some platforms screen readers should not stop when
  // encountering a splitter during character and word navigation.
  bool IsIgnoredForTextNavigation() const;

  // Returns true if node is invisible or ignored.
  bool IsInvisibleOrIgnored() const;

  // Returns true if node is focused within this tree.
  bool IsFocusedWithinThisTree() const;

  // Returns true if an ancestor of this node (not including itself) is a
  // leaf node, meaning that this node is not actually exposed to any
  // platform's accessibility layer.
  bool IsChildOfLeaf() const;

  // Returns true if this is a leaf node that has no inner text. Note that all
  // descendants of a leaf node are not exposed to any platform's accessibility
  // layer, but they may be used to compute the node's inner text. Note also
  // that, ignored nodes (leaf or otherwise) do not expose their inner text or
  // hypertext to the platforms' accessibility layer, but they expose the inner
  // text or hypertext of their unignored descendants.
  //
  // For example, empty text fields might have a set of unignored nested divs
  // inside them:
  // ++kTextField
  // ++++kGenericContainer
  // ++++++kGenericContainer
  bool IsEmptyLeaf() const;

  // Returns true if this is a leaf node, meaning all its
  // descendants should not be exposed to any platform's accessibility
  // layer.
  //
  // The definition of a leaf includes nodes with children that are exclusively
  // an internal renderer implementation, such as the children of an HTML-based
  // text field (<input> and <textarea>), as well as nodes with presentational
  // children according to the ARIA and HTML5 Specs. Also returns true if all of
  // the node's descendants are ignored.
  //
  // A leaf node should never have children that are focusable or
  // that might send notifications.
  bool IsLeaf() const;

  // Returns true if this node is a list marker or if it's a descendant
  // of a list marker node. Returns false otherwise.
  bool IsInListMarker() const;

  // Returns true if this node is a collapsed popup button that is parent to a
  // menu list popup.
  bool IsCollapsedMenuListPopUpButton() const;

  // Returns the popup button ancestor of this current node if any. The popup
  // button needs to be the parent of a menu list popup and needs to be
  // collapsed.
  AXNode* GetCollapsedMenuListPopUpButtonAncestor() const;

  // If this node is exposed to the platform's accessibility layer, returns this
  // node. Otherwise, returns the lowest ancestor that is exposed to the
  // platform. (See `IsLeaf()` and `IsIgnored()` for information on what is
  // exposed to platform APIs.)
  AXNode* GetLowestPlatformAncestor() const;

  // If this node is within an editable region, returns the node that is at the
  // root of that editable region, otherwise returns nullptr. In accessibility,
  // an editable region is synonymous to a node with the kTextField role, or a
  // contenteditable without the role, (see `AXNodeData::IsTextField()`).
  AXNode* GetTextFieldAncestor() const;

  // Returns true if this node is either an atomic text field , or one of its
  // ancestors is. An atomic text field does not expose its internal
  // implementation to assistive software, appearing as a single leaf node in
  // the accessibility tree. Examples include: An <input> or a <textarea> on the
  // Web, a text field in a PDF form, a Views-based text field, or a native
  // Android one.
  bool IsDescendantOfAtomicTextField() const;

  // Finds and returns a pointer to ordered set containing node.
  AXNode* GetOrderedSet() const;

 private:
  // Computes the text offset where each line starts by traversing all child
  // leaf nodes.
  void ComputeLineStartOffsets(std::vector<int>* line_offsets,
                               int* start_offset) const;
  AXTableInfo* GetAncestorTableInfo() const;
  void IdVectorToNodeVector(const std::vector<AXNodeID>& ids,
                            std::vector<AXNode*>* nodes) const;

  int UpdateUnignoredCachedValuesRecursive(int start_index);
  AXNode* ComputeLastUnignoredChildRecursive() const;
  AXNode* ComputeFirstUnignoredChildRecursive() const;

  // Returns the value of a range control such as a slider or a scroll bar in
  // text format.
  std::string GetTextForRangeValue() const;

  // Returns the value of a color well (a color chooser control) in a human
  // readable format. For example: "50% red 40% green 90% blue".
  std::string GetValueForColorWell() const;

  // Returns the value of a text field. If necessary, computes the value from
  // the field's internal representation in the accessibility tree, in order to
  // minimize cross-process communication between the renderer and the browser
  // processes.
  std::string GetValueForTextField() const;

  // Compute the actual value of a color attribute that needs to be
  // blended with ancestor colors.
  SkColor ComputeColorAttribute(ax::mojom::IntAttribute color_attr) const;

  OwnerTree* const tree_;  // Owns this.
  size_t index_in_parent_;
  size_t unignored_index_in_parent_;
  size_t unignored_child_count_ = 0;
  AXNode* const parent_;
  std::vector<AXNode*> children_;
  AXNodeData data_;

  // See the class comment in "ax_hypertext.h" for an explanation of this
  // member.
  mutable AXHypertext hypertext_;
  mutable AXHypertext old_hypertext_;

  // Stores the detected language computed from the node's text.
  std::unique_ptr<AXLanguageInfo> language_info_;
};

AX_EXPORT std::ostream& operator<<(std::ostream& stream, const AXNode& node);

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
AXNode::ChildIteratorBase<NodeType,
                          NextSibling,
                          PreviousSibling,
                          FirstChild,
                          LastChild>::ChildIteratorBase(const NodeType* parent,
                                                        NodeType* child)
    : parent_(parent), child_(child) {}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
AXNode::ChildIteratorBase<NodeType,
                          NextSibling,
                          PreviousSibling,
                          FirstChild,
                          LastChild>::ChildIteratorBase(const ChildIteratorBase&
                                                            it)
    : parent_(it.parent_), child_(it.child_) {}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
bool AXNode::ChildIteratorBase<NodeType,
                               NextSibling,
                               PreviousSibling,
                               FirstChild,
                               LastChild>::operator==(const ChildIteratorBase&
                                                          rhs) const {
  return parent_ == rhs.parent_ && child_ == rhs.child_;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
bool AXNode::ChildIteratorBase<NodeType,
                               NextSibling,
                               PreviousSibling,
                               FirstChild,
                               LastChild>::operator!=(const ChildIteratorBase&
                                                          rhs) const {
  return parent_ != rhs.parent_ || child_ != rhs.child_;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
AXNode::ChildIteratorBase<NodeType,
                          NextSibling,
                          PreviousSibling,
                          FirstChild,
                          LastChild>&
AXNode::ChildIteratorBase<NodeType,
                          NextSibling,
                          PreviousSibling,
                          FirstChild,
                          LastChild>::operator++() {
  // |child_ = nullptr| denotes the iterator's past-the-end condition. When we
  // increment the iterator past the end, we remain at the past-the-end iterator
  // condition.
  if (child_ && parent_) {
    if (child_ == (parent_->*LastChild)())
      child_ = nullptr;
    else
      child_ = (child_->*NextSibling)();
  }

  return *this;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
AXNode::ChildIteratorBase<NodeType,
                          NextSibling,
                          PreviousSibling,
                          FirstChild,
                          LastChild>&
AXNode::ChildIteratorBase<NodeType,
                          NextSibling,
                          PreviousSibling,
                          FirstChild,
                          LastChild>::operator--() {
  if (parent_) {
    // If the iterator is past the end, |child_=nullptr|, decrement the iterator
    // gives us the last iterator element.
    if (!child_)
      child_ = (parent_->*LastChild)();
    // Decrement the iterator gives us the previous element, except when the
    // iterator is at the beginning; in which case, decrementing the iterator
    // remains at the beginning.
    else if (child_ != (parent_->*FirstChild)())
      child_ = (child_->*PreviousSibling)();
  }

  return *this;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
NodeType* AXNode::ChildIteratorBase<NodeType,
                                    NextSibling,
                                    PreviousSibling,
                                    FirstChild,
                                    LastChild>::get() const {
  DCHECK(child_);
  return child_;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
NodeType& AXNode::ChildIteratorBase<NodeType,
                                    NextSibling,
                                    PreviousSibling,
                                    FirstChild,
                                    LastChild>::operator*() const {
  DCHECK(child_);
  return *child_;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*FirstChild)() const,
          NodeType* (NodeType::*LastChild)() const>
NodeType* AXNode::ChildIteratorBase<NodeType,
                                    NextSibling,
                                    PreviousSibling,
                                    FirstChild,
                                    LastChild>::operator->() const {
  DCHECK(child_);
  return child_;
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_NODE_H_
