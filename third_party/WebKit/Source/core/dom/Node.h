/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2011, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef Node_h
#define Node_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/dom/MutationObserver.h"
#include "core/dom/TreeScope.h"
#include "core/dom/events/EventTarget.h"
#include "core/dom/events/SimulatedClickOptions.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/geometry/LayoutRect.h"
#include "public/platform/WebFocusType.h"

// This needs to be here because Element.cpp also depends on it.
#define DUMP_NODE_STATISTICS 0

namespace blink {

class ComputedStyle;
class ContainerNode;
class Document;
class Element;
class ElementShadow;
class Event;
class EventDispatchHandlingState;
class ExceptionState;
class GetRootNodeOptions;
class HTMLQualifiedName;
class HTMLSlotElement;
class IntRect;
class KURL;
class LayoutBox;
class LayoutBoxModelObject;
class LayoutObject;
class NodeList;
class NodeListsNodeData;
class NodeOrString;
class NodeRareData;
class QualifiedName;
class RegisteredEventListener;
class SVGQualifiedName;
class ShadowRoot;
template <typename NodeType>
class StaticNodeTypeList;
using StaticNodeList = StaticNodeTypeList<Node>;
class StyleChangeReasonForTracing;
class WebMouseEvent;
class WebPluginContainerImpl;

const int kNodeStyleChangeShift = 18;
const int kNodeCustomElementShift = 20;

enum StyleChangeType {
  kNoStyleChange = 0,
  kLocalStyleChange = 1 << kNodeStyleChangeShift,
  kSubtreeStyleChange = 2 << kNodeStyleChangeShift,
  kNeedsReattachStyleChange = 3 << kNodeStyleChangeShift,
};

enum class CustomElementState {
  // https://dom.spec.whatwg.org/#concept-element-custom-element-state
  kUncustomized = 0,
  kCustom = 1 << kNodeCustomElementShift,
  kUndefined = 2 << kNodeCustomElementShift,
  kFailed = 3 << kNodeCustomElementShift,

  kNotDefinedFlag = 2 << kNodeCustomElementShift,
};

enum class SlotChangeType {
  kSignalSlotChangeEvent,
  kSuppressSlotChangeEvent,
};

enum class CloneChildrenFlag { kClone, kSkip };

class NodeRenderingData {
 public:
  explicit NodeRenderingData(LayoutObject* layout_object,
                             scoped_refptr<ComputedStyle> non_attached_style);
  ~NodeRenderingData();

  LayoutObject* GetLayoutObject() const { return layout_object_; }
  void SetLayoutObject(LayoutObject* layout_object) {
    DCHECK_NE(&SharedEmptyData(), this);
    layout_object_ = layout_object;
  }

  ComputedStyle* GetNonAttachedStyle() const {
    return non_attached_style_.get();
  }
  void SetNonAttachedStyle(scoped_refptr<ComputedStyle> non_attached_style);

  static NodeRenderingData& SharedEmptyData();
  bool IsSharedEmptyData() { return this == &SharedEmptyData(); }

 private:
  LayoutObject* layout_object_;
  scoped_refptr<ComputedStyle> non_attached_style_;
  DISALLOW_COPY_AND_ASSIGN(NodeRenderingData);
};

class NodeRareDataBase {
 public:
  NodeRenderingData* GetNodeRenderingData() const { return node_layout_data_; }
  void SetNodeRenderingData(NodeRenderingData* node_layout_data) {
    DCHECK(node_layout_data);
    node_layout_data_ = node_layout_data;
  }

 protected:
  explicit NodeRareDataBase(NodeRenderingData* node_layout_data)
      : node_layout_data_(node_layout_data) {}
  ~NodeRareDataBase() {
    if (node_layout_data_ && !node_layout_data_->IsSharedEmptyData())
      delete node_layout_data_;
  }

 protected:
  NodeRenderingData* node_layout_data_;
};

class Node;
WILL_NOT_BE_EAGERLY_TRACED_CLASS(Node);

// A Node is a base class for all objects in the DOM tree.
// The spec governing this interface can be found here:
// https://dom.spec.whatwg.org/#interface-node
class CORE_EXPORT Node : public EventTarget {
  DEFINE_WRAPPERTYPEINFO();
  friend class TreeScope;
  friend class TreeScopeAdopter;

 public:
  enum NodeType {
    kElementNode = 1,
    kAttributeNode = 2,
    kTextNode = 3,
    kCdataSectionNode = 4,
    kProcessingInstructionNode = 7,
    kCommentNode = 8,
    kDocumentNode = 9,
    kDocumentTypeNode = 10,
    kDocumentFragmentNode = 11,
  };

  // Entity, EntityReference, and Notation nodes are impossible to create in
  // Blink.  But for compatibility reasons we want these enum values exist in
  // JS, and this enum makes the bindings generation not complain about
  // kEntityReferenceNode being missing from the implementation while not
  // requiring all switch(NodeType) blocks to include this deprecated constant.
  enum DeprecatedNodeType {
    kEntityReferenceNode = 5,
    kEntityNode = 6,
    kNotationNode = 12,
  };

  enum DocumentPosition {
    kDocumentPositionEquivalent = 0x00,
    kDocumentPositionDisconnected = 0x01,
    kDocumentPositionPreceding = 0x02,
    kDocumentPositionFollowing = 0x04,
    kDocumentPositionContains = 0x08,
    kDocumentPositionContainedBy = 0x10,
    kDocumentPositionImplementationSpecific = 0x20,
  };

  // Override operator new to allocate Node subtype objects onto
  // a dedicated heap.
  GC_PLUGIN_IGNORE("crbug.com/443854")
  void* operator new(size_t size) { return AllocateObject(size, false); }
  static void* AllocateObject(size_t size, bool is_eager) {
    ThreadState* state =
        ThreadStateFor<ThreadingTrait<Node>::kAffinity>::GetState();
    const char* type_name = "blink::Node";
    return state->Heap().AllocateOnArenaIndex(
        state, size,
        is_eager ? BlinkGC::kEagerSweepArenaIndex : BlinkGC::kNodeArenaIndex,
        GCInfoTrait<EventTarget>::Index(), type_name);
  }

  static void DumpStatistics();

  ~Node() override;

  // DOM methods & attributes for Node

  bool HasTagName(const HTMLQualifiedName&) const;
  bool HasTagName(const SVGQualifiedName&) const;
  virtual String nodeName() const = 0;
  virtual String nodeValue() const;
  virtual void setNodeValue(const String&);
  virtual NodeType getNodeType() const = 0;
  ContainerNode* parentNode() const;
  Element* parentElement() const;
  ContainerNode* ParentElementOrShadowRoot() const;
  ContainerNode* ParentElementOrDocumentFragment() const;
  Node* previousSibling() const { return previous_; }
  Node* nextSibling() const { return next_; }
  NodeList* childNodes();
  Node* firstChild() const;
  Node* lastChild() const;
  Node* getRootNode(const GetRootNodeOptions&) const;
  Node& TreeRoot() const;
  Node& ShadowIncludingRoot() const;
  // closed-shadow-hidden is defined at
  // https://dom.spec.whatwg.org/#concept-closed-shadow-hidden
  bool IsClosedShadowHiddenFrom(const Node&) const;

  void Prepend(const HeapVector<NodeOrString>&, ExceptionState&);
  void Append(const HeapVector<NodeOrString>&, ExceptionState&);
  void Before(const HeapVector<NodeOrString>&, ExceptionState&);
  void After(const HeapVector<NodeOrString>&, ExceptionState&);
  void ReplaceWith(const HeapVector<NodeOrString>&, ExceptionState&);
  void remove(ExceptionState&);
  void remove();

  Node* PseudoAwareNextSibling() const;
  Node* PseudoAwarePreviousSibling() const;
  Node* PseudoAwareFirstChild() const;
  Node* PseudoAwareLastChild() const;

  const KURL& baseURI() const;

  Node* insertBefore(Node* new_child, Node* ref_child, ExceptionState&);
  Node* insertBefore(Node* new_child, Node* ref_child);
  Node* replaceChild(Node* new_child, Node* old_child, ExceptionState&);
  Node* replaceChild(Node* new_child, Node* old_child);
  Node* removeChild(Node* child, ExceptionState&);
  Node* removeChild(Node* child);
  Node* appendChild(Node* new_child, ExceptionState&);
  Node* appendChild(Node* new_child);

  bool hasChildren() const { return firstChild(); }
  Node* cloneNode(bool deep, ExceptionState&) const;
  // https://dom.spec.whatwg.org/#concept-node-clone
  virtual Node* Clone(Document&, CloneChildrenFlag) const = 0;
  // This is not web-exposed. We should rename it or remove it.
  Node* cloneNode(bool deep) const;
  void normalize();

  bool isEqualNode(Node*) const;
  bool isSameNode(const Node* other) const { return this == other; }
  bool isDefaultNamespace(const AtomicString& namespace_uri) const;
  const AtomicString& lookupPrefix(const AtomicString& namespace_uri) const;
  const AtomicString& lookupNamespaceURI(const String& prefix) const;

  String textContent(bool convert_brs_to_newlines = false) const;
  void setTextContent(const String&);

  bool SupportsAltText();

  void SetNonAttachedStyle(scoped_refptr<ComputedStyle> non_attached_style);

  ComputedStyle* GetNonAttachedStyle() const {
    return HasRareData()
               ? data_.rare_data_->GetNodeRenderingData()->GetNonAttachedStyle()
               : data_.node_layout_data_->GetNonAttachedStyle();
  }

  // Other methods (not part of DOM)

  bool IsElementNode() const { return GetFlag(kIsElementFlag); }
  bool IsContainerNode() const { return GetFlag(kIsContainerFlag); }
  bool IsTextNode() const { return GetFlag(kIsTextFlag); }
  bool IsHTMLElement() const { return GetFlag(kIsHTMLFlag); }
  bool IsSVGElement() const { return GetFlag(kIsSVGFlag); }

  DISABLE_CFI_PERF bool IsPseudoElement() const {
    return GetPseudoId() != kPseudoIdNone;
  }
  DISABLE_CFI_PERF bool IsBeforePseudoElement() const {
    return GetPseudoId() == kPseudoIdBefore;
  }
  DISABLE_CFI_PERF bool IsAfterPseudoElement() const {
    return GetPseudoId() == kPseudoIdAfter;
  }
  DISABLE_CFI_PERF bool IsFirstLetterPseudoElement() const {
    return GetPseudoId() == kPseudoIdFirstLetter;
  }
  virtual PseudoId GetPseudoId() const { return kPseudoIdNone; }

  CustomElementState GetCustomElementState() const {
    return static_cast<CustomElementState>(node_flags_ &
                                           kCustomElementStateMask);
  }
  void SetCustomElementState(CustomElementState);
  bool IsV0CustomElement() const { return GetFlag(kV0CustomElementFlag); }
  enum V0CustomElementState {
    kV0NotCustomElement = 0,
    kV0WaitingForUpgrade = 1 << 0,
    kV0Upgraded = 1 << 1
  };
  V0CustomElementState GetV0CustomElementState() const {
    return IsV0CustomElement()
               ? (GetFlag(kV0CustomElementUpgradedFlag) ? kV0Upgraded
                                                        : kV0WaitingForUpgrade)
               : kV0NotCustomElement;
  }
  void SetV0CustomElementState(V0CustomElementState new_state);

  virtual bool IsMediaControlElement() const { return false; }
  virtual bool IsMediaControls() const { return false; }
  virtual bool IsTextTrackContainer() const { return false; }
  virtual bool IsVTTElement() const { return false; }
  virtual bool IsAttributeNode() const { return false; }
  virtual bool IsCharacterDataNode() const { return false; }
  virtual bool IsFrameOwnerElement() const { return false; }
  virtual bool IsMediaRemotingInterstitial() const { return false; }
  virtual bool IsPictureInPictureInterstitial() const { return false; }

  // Traverses the ancestors of this node and returns true if any of them are
  // either a MediaControlElement or MediaControls.
  bool HasMediaControlAncestor() const;

  bool IsStyledElement() const;

  bool IsDocumentNode() const;
  bool IsTreeScope() const;
  bool IsDocumentFragment() const { return GetFlag(kIsDocumentFragmentFlag); }
  bool IsShadowRoot() const { return IsDocumentFragment() && IsTreeScope(); }
  bool IsV0InsertionPoint() const { return GetFlag(kIsV0InsertionPointFlag); }

  bool CanParticipateInFlatTree() const;
  bool IsActiveSlotOrActiveV0InsertionPoint() const;
  // A re-distribution across v0 and v1 shadow trees is not supported.
  bool IsSlotable() const {
    return IsTextNode() || (IsElementNode() && !IsV0InsertionPoint());
  }
  AtomicString SlotName() const;

  bool HasCustomStyleCallbacks() const {
    return GetFlag(kHasCustomStyleCallbacksFlag);
  }

  // If this node is in a shadow tree, returns its shadow host. Otherwise,
  // returns nullptr.
  // TODO(kochi): crbug.com/507413 ownerShadowHost() can return nullptr even
  // when it is in a shadow tree but its root is detached from its host. This
  // can happen when handling queued events (e.g. during execCommand()).
  Element* OwnerShadowHost() const;
  // crbug.com/569532: containingShadowRoot() can return nullptr even if
  // isInShadowTree() returns true.
  // This can happen when handling queued events (e.g. during execCommand())
  ShadowRoot* ContainingShadowRoot() const;
  ShadowRoot* GetShadowRoot() const;
  bool IsInUserAgentShadowRoot() const;

  // Returns nullptr, a child of ShadowRoot, or a legacy shadow root.
  Node* NonBoundaryShadowTreeRootNode();

  // Node's parent, shadow tree host.
  ContainerNode* ParentOrShadowHostNode() const;
  Element* ParentOrShadowHostElement() const;
  void SetParentOrShadowHostNode(ContainerNode*);

  // Knows about all kinds of hosts.
  ContainerNode* ParentOrShadowHostOrTemplateHostNode() const;

  // Returns the parent node, but nullptr if the parent node is a ShadowRoot.
  ContainerNode* NonShadowBoundaryParentNode() const;

  // Returns the enclosing event parent Element (or self) that, when clicked,
  // would trigger a navigation.
  Element* EnclosingLinkEventParentOrSelf() const;

  // These low-level calls give the caller responsibility for maintaining the
  // integrity of the tree.
  void SetPreviousSibling(Node* previous) {
    previous_ = previous;
  }
  void SetNextSibling(Node* next) {
    next_ = next;
  }

  virtual bool CanContainRangeEndPoint() const { return false; }

  // For <link> and <style> elements.
  virtual bool SheetLoaded() { return true; }
  enum LoadedSheetErrorStatus {
    kNoErrorLoadingSubresource,
    kErrorOccurredLoadingSubresource
  };
  virtual void NotifyLoadedSheetAndAllCriticalSubresources(
      LoadedSheetErrorStatus) {}
  virtual void StartLoadingDynamicSheet() { NOTREACHED(); }

  bool HasName() const {
    DCHECK(!IsTextNode());
    return GetFlag(kHasNameOrIsEditingTextFlag);
  }

  bool IsUserActionElement() const { return GetFlag(kIsUserActionElementFlag); }
  void SetUserActionElement(bool flag) {
    SetFlag(flag, kIsUserActionElementFlag);
  }

  bool IsActive() const {
    return IsUserActionElement() && IsUserActionElementActive();
  }
  bool InActiveChain() const {
    return IsUserActionElement() && IsUserActionElementInActiveChain();
  }
  bool IsDragged() const {
    return IsUserActionElement() && IsUserActionElementDragged();
  }
  bool IsHovered() const {
    return IsUserActionElement() && IsUserActionElementHovered();
  }
  // Note: As a shadow host whose root with delegatesFocus=false may become
  // focused state when an inner element gets focused, in that case more than
  // one elements in a document can return true for |isFocused()|.  Use
  // Element::isFocusedElementInDocument() or Document::focusedElement() to
  // check which element is exactly focused.
  bool IsFocused() const {
    return IsUserActionElement() && IsUserActionElementFocused();
  }
  bool HasFocusWithin() const {
    return IsUserActionElement() && IsUserActionElementHasFocusWithin();
  }

  bool NeedsAttach() const {
    return GetStyleChangeType() == kNeedsReattachStyleChange;
  }
  bool NeedsStyleRecalc() const {
    return GetStyleChangeType() != kNoStyleChange;
  }
  StyleChangeType GetStyleChangeType() const {
    return static_cast<StyleChangeType>(node_flags_ & kStyleChangeMask);
  }
  bool ChildNeedsStyleRecalc() const {
    return GetFlag(kChildNeedsStyleRecalcFlag);
  }
  bool IsLink() const { return GetFlag(kIsLinkFlag); }
  bool IsEditingText() const {
    DCHECK(IsTextNode());
    return GetFlag(kHasNameOrIsEditingTextFlag);
  }

  void SetHasName(bool f) {
    DCHECK(!IsTextNode());
    SetFlag(f, kHasNameOrIsEditingTextFlag);
  }
  void SetChildNeedsStyleRecalc() { SetFlag(kChildNeedsStyleRecalcFlag); }
  void ClearChildNeedsStyleRecalc() { ClearFlag(kChildNeedsStyleRecalcFlag); }

  void SetNeedsStyleRecalc(StyleChangeType, const StyleChangeReasonForTracing&);
  void ClearNeedsStyleRecalc();

  bool NeedsReattachLayoutTree() const {
    return GetFlag(kNeedsReattachLayoutTree);
  }
  bool ChildNeedsReattachLayoutTree() const {
    return GetFlag(kChildNeedsReattachLayoutTree);
  }

  void SetNeedsReattachLayoutTree();
  void SetChildNeedsReattachLayoutTree() {
    SetFlag(kChildNeedsReattachLayoutTree);
  }

  void ClearNeedsReattachLayoutTree() { ClearFlag(kNeedsReattachLayoutTree); }
  void ClearChildNeedsReattachLayoutTree() {
    ClearFlag(kChildNeedsReattachLayoutTree);
  }

  void MarkAncestorsWithChildNeedsReattachLayoutTree();

  bool NeedsDistributionRecalc() const;

  bool ChildNeedsDistributionRecalc() const {
    return GetFlag(kChildNeedsDistributionRecalcFlag);
  }
  void SetChildNeedsDistributionRecalc() {
    SetFlag(kChildNeedsDistributionRecalcFlag);
  }
  void ClearChildNeedsDistributionRecalc() {
    ClearFlag(kChildNeedsDistributionRecalcFlag);
  }
  void MarkAncestorsWithChildNeedsDistributionRecalc();

  bool ChildNeedsStyleInvalidation() const {
    return GetFlag(kChildNeedsStyleInvalidationFlag);
  }
  void SetChildNeedsStyleInvalidation() {
    SetFlag(kChildNeedsStyleInvalidationFlag);
  }
  void ClearChildNeedsStyleInvalidation() {
    ClearFlag(kChildNeedsStyleInvalidationFlag);
  }
  void MarkAncestorsWithChildNeedsStyleInvalidation();
  bool NeedsStyleInvalidation() const {
    return GetFlag(kNeedsStyleInvalidationFlag);
  }
  void ClearNeedsStyleInvalidation() { ClearFlag(kNeedsStyleInvalidationFlag); }
  void SetNeedsStyleInvalidation();

  void UpdateDistribution();
  bool MayContainLegacyNodeTreeWhereDistributionShouldBeSupported() const;

  void SetIsLink(bool f);

  bool HasEventTargetData() const { return GetFlag(kHasEventTargetDataFlag); }
  void SetHasEventTargetData(bool flag) {
    SetFlag(flag, kHasEventTargetDataFlag);
  }

  virtual void SetFocused(bool flag, WebFocusType);
  virtual void SetHasFocusWithin(bool flag);
  virtual void SetActive(bool flag = true);
  virtual void SetDragged(bool flag);
  virtual void SetHovered(bool flag = true);

  virtual int tabIndex() const;

  virtual const Node* FocusDelegate() const;
  // This is called only when the node is focused.
  virtual bool ShouldHaveFocusAppearance() const;

  // Whether the node is inert:
  // https://html.spec.whatwg.org/multipage/interaction.html#inert
  // https://github.com/WICG/inert/blob/master/README.md
  // This can't be in Element because text nodes must be recognized as
  // inert to prevent text selection.
  bool IsInert() const;

  virtual LayoutRect BoundingBox() const;
  IntRect PixelSnappedBoundingBox() const {
    return PixelSnappedIntRect(BoundingBox());
  }

  // BoundingBoxForScrollIntoView() is the node's scroll snap area.
  // It is expanded from the BoundingBox() by scroll-margin.
  // https://drafts.csswg.org/css-scroll-snap-1/#scroll-snap-area
  LayoutRect BoundingBoxForScrollIntoView() const;

  unsigned NodeIndex() const;

  // Returns the DOM ownerDocument attribute. This method never returns null,
  // except in the case of a Document node.
  Document* ownerDocument() const;

  // Returns the document associated with this node. A Document node returns
  // itself.
  Document& GetDocument() const { return GetTreeScope().GetDocument(); }

  TreeScope& GetTreeScope() const {
    DCHECK(tree_scope_);
    return *tree_scope_;
  }

  TreeScope& ContainingTreeScope() const {
    DCHECK(IsInTreeScope());
    return *tree_scope_;
  }

  bool InActiveDocument() const;

  // Returns true if this node is connected to a document, false otherwise.
  // See https://dom.spec.whatwg.org/#connected for the definition.
  bool isConnected() const { return GetFlag(kIsConnectedFlag); }

  bool IsInDocumentTree() const { return isConnected() && !IsInShadowTree(); }
  bool IsInShadowTree() const { return GetFlag(kIsInShadowTreeFlag); }
  bool IsInTreeScope() const {
    return GetFlag(
        static_cast<NodeFlags>(kIsConnectedFlag | kIsInShadowTreeFlag));
  }

  ElementShadow* ParentElementShadow() const;
  bool IsInV1ShadowTree() const;
  bool IsInV0ShadowTree() const;
  bool IsChildOfV1ShadowHost() const;
  bool IsChildOfV0ShadowHost() const;
  ShadowRoot* V1ShadowRootOfParent() const;

  bool IsDocumentTypeNode() const { return getNodeType() == kDocumentTypeNode; }
  virtual bool ChildTypeAllowed(NodeType) const { return false; }
  unsigned CountChildren() const;

  bool IsDescendantOf(const Node*) const;
  bool contains(const Node*) const;
  bool IsShadowIncludingInclusiveAncestorOf(const Node*) const;
  bool ContainsIncludingHostElements(const Node&) const;
  Node* CommonAncestor(const Node&,
                       ContainerNode* (*parent)(const Node&)) const;

  // Number of DOM 16-bit units contained in node. Note that laid out text
  // length can be different - e.g. because of css-transform:capitalize breaking
  // up precomposed characters and ligatures.
  virtual int MaxCharacterOffset() const;

  // Whether or not a selection can be started in this object
  virtual bool CanStartSelection() const;

  // -----------------------------------------------------------------------------
  // Integration with layout tree

  // As layoutObject() includes a branch you should avoid calling it repeatedly
  // in hot code paths.
  // Note that if a Node has a layoutObject, it's parentNode is guaranteed to
  // have one as well.
  LayoutObject* GetLayoutObject() const {
    return HasRareData()
               ? data_.rare_data_->GetNodeRenderingData()->GetLayoutObject()
               : data_.node_layout_data_->GetLayoutObject();
  }
  void SetLayoutObject(LayoutObject*);
  // Use these two methods with caution.
  LayoutBox* GetLayoutBox() const;
  LayoutBoxModelObject* GetLayoutBoxModelObject() const;

  struct AttachContext {
    STACK_ALLOCATED();
    // Keep track of previously attached in-flow box during attachment so that
    // we don't need to backtrack past display:none/contents and out of flow
    // objects when we need to do whitespace re-attachment.
    LayoutObject* previous_in_flow = nullptr;
    bool performing_reattach = false;
    bool clear_invalidation = false;
    // True if the previous_in_flow member is up-to-date, even if it is nullptr.
    bool use_previous_in_flow = false;

    AttachContext() {}
  };

  // Attaches this node to the layout tree. This calculates the style to be
  // applied to the node and creates an appropriate LayoutObject which will be
  // inserted into the tree (except when the style has display: none). This
  // makes the node visible in the LocalFrameView.
  virtual void AttachLayoutTree(AttachContext&);

  // Detaches the node from the layout tree, making it invisible in the rendered
  // view. This method will remove the node's layout object from the layout tree
  // and delete it.
  virtual void DetachLayoutTree(const AttachContext& = AttachContext());

  void ReattachLayoutTree() {
    AttachContext context;
    ReattachLayoutTree(context);
  }
  void ReattachLayoutTree(AttachContext&);
  void LazyReattachIfAttached();

  // Returns true if recalcStyle should be called on the object, if there is
  // such a method (on Document and Element).
  bool ShouldCallRecalcStyle(StyleRecalcChange);

  // Wrapper for nodes that don't have a layoutObject, but still cache the style
  // (like HTMLOptionElement).
  ComputedStyle* MutableComputedStyle() const;
  const ComputedStyle* GetComputedStyle() const;
  const ComputedStyle* ParentComputedStyle() const;

  const ComputedStyle& ComputedStyleRef() const;

  const ComputedStyle* EnsureComputedStyle(
      PseudoId pseudo_element_specifier = kPseudoIdNone) {
    return VirtualEnsureComputedStyle(pseudo_element_specifier);
  }

  // -----------------------------------------------------------------------------
  // Notification of document structure changes (see ContainerNode.h for more
  // notification methods)
  //
  // At first, Blinkt notifies the node that it has been inserted into the
  // document. This is called during document parsing, and also when a node is
  // added through the DOM methods insertBefore(), appendChild() or
  // replaceChild(). The call happens _after_ the node has been added to the
  // tree.  This is similar to the DOMNodeInsertedIntoDocument DOM event, but
  // does not require the overhead of event dispatching.
  //
  // Blink notifies this callback regardless if the subtree of the node is a
  // document tree or a floating subtree.  Implementation can determine the type
  // of subtree by seeing insertionPoint->isConnected().  For a performance
  // reason, notifications are delivered only to ContainerNode subclasses if the
  // insertionPoint is out of document.
  //
  // There are another callback named didNotifySubtreeInsertionsToDocument(),
  // which is called after all the descendant is notified, if this node was
  // inserted into the document tree. Only a few subclasses actually need
  // this. To utilize this, the node should return
  // InsertionShouldCallDidNotifySubtreeInsertions from insertedInto().
  //
  enum InsertionNotificationRequest {
    kInsertionDone,
    kInsertionShouldCallDidNotifySubtreeInsertions
  };

  virtual InsertionNotificationRequest InsertedInto(
      ContainerNode* insertion_point);
  virtual void DidNotifySubtreeInsertionsToDocument() {}

  // Notifies the node that it is no longer part of the tree.
  //
  // This is a dual of insertedInto(), and is similar to the
  // DOMNodeRemovedFromDocument DOM event, but does not require the overhead of
  // event dispatching, and is called _after_ the node is removed from the tree.
  //
  virtual void RemovedFrom(ContainerNode* insertion_point);

  // FIXME(dominicc): This method is not debug-only--it is used by
  // Tracing--rename it to something indicative.
  String DebugName() const;

  String ToString() const;

#ifndef NDEBUG
  String ToTreeStringForThis() const;
  String ToFlatTreeStringForThis() const;
  void PrintNodePathTo(std::ostream&) const;
  String ToMarkedTreeString(const Node* marked_node1,
                            const char* marked_label1,
                            const Node* marked_node2 = nullptr,
                            const char* marked_label2 = nullptr) const;
  String ToMarkedFlatTreeString(const Node* marked_node1,
                                const char* marked_label1,
                                const Node* marked_node2 = nullptr,
                                const char* marked_label2 = nullptr) const;
  void ShowTreeForThisAcrossFrame() const;
#endif

  NodeListsNodeData* NodeLists();
  void ClearNodeLists();

  virtual bool WillRespondToMouseMoveEvents();
  virtual bool WillRespondToMouseClickEvents();
  virtual bool WillRespondToTouchEvents();

  enum ShadowTreesTreatment {
    kTreatShadowTreesAsDisconnected,
    kTreatShadowTreesAsComposed
  };

  unsigned short compareDocumentPosition(
      const Node*,
      ShadowTreesTreatment = kTreatShadowTreesAsDisconnected) const;

  Node* ToNode() final;

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const final;

  void RemoveAllEventListeners() override;
  void RemoveAllEventListenersRecursively();

  // Handlers to do/undo actions on the target node before an event is
  // dispatched to it and after the event has been dispatched.  The data pointer
  // is handed back by the preDispatch and passed to postDispatch.
  virtual EventDispatchHandlingState* PreDispatchEventHandler(Event*) {
    return nullptr;
  }
  virtual void PostDispatchEventHandler(Event*, EventDispatchHandlingState*) {}

  void DispatchScopedEvent(Event*);

  virtual void HandleLocalEvents(Event&);

  void DispatchSubtreeModifiedEvent();
  DispatchEventResult DispatchDOMActivateEvent(int detail,
                                               Event& underlying_event);

  void DispatchMouseEvent(const WebMouseEvent&,
                          const AtomicString& event_type,
                          int click_count = 0,
                          const String& canvas_node_id = String(),
                          Node* related_target = nullptr);

  void DispatchSimulatedClick(Event* underlying_event,
                              SimulatedClickMouseEventOptions = kSendNoEvents,
                              SimulatedClickCreationScope =
                                  SimulatedClickCreationScope::kFromUserAgent);

  void DispatchInputEvent();

  // Perform the default action for an event.
  virtual void DefaultEventHandler(Event*);
  virtual void WillCallDefaultEventHandler(const Event&);
  // Should return true if this Node has activation behavior.
  // https://dom.spec.whatwg.org/#eventtarget-activation-behavior
  virtual bool HasActivationBehavior() const;

  EventTargetData* GetEventTargetData() override;
  EventTargetData& EnsureEventTargetData() override;

  void GetRegisteredMutationObserversOfType(
      HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>&,
      MutationObserver::MutationType,
      const QualifiedName* attribute_name);
  void RegisterMutationObserver(MutationObserver&,
                                MutationObserverOptions,
                                const HashSet<AtomicString>& attribute_filter);
  void UnregisterMutationObserver(MutationObserverRegistration*);
  void RegisterTransientMutationObserver(MutationObserverRegistration*);
  void UnregisterTransientMutationObserver(MutationObserverRegistration*);
  void NotifyMutationObserversNodeWillDetach();

  unsigned ConnectedSubframeCount() const;
  void IncrementConnectedSubframeCount();
  void DecrementConnectedSubframeCount();

  StaticNodeList* getDestinationInsertionPoints();
  HTMLSlotElement* AssignedSlot() const;
  HTMLSlotElement* FinalDestinationSlot() const;
  HTMLSlotElement* assignedSlotForBinding();

  bool IsFinishedParsingChildren() const {
    return GetFlag(kIsFinishedParsingChildrenFlag);
  }

  void CheckSlotChange(SlotChangeType);
  void CheckSlotChangeAfterInserted() {
    CheckSlotChange(SlotChangeType::kSignalSlotChangeEvent);
  }
  void CheckSlotChangeBeforeRemoved() {
    CheckSlotChange(SlotChangeType::kSignalSlotChangeEvent);
  }

  void SetHasDuplicateAttributes() { SetFlag(kHasDuplicateAttributes); }
  bool HasDuplicateAttribute() const {
    return GetFlag(kHasDuplicateAttributes);
  }

  // If the node is a plugin, then this returns its WebPluginContainer.
  WebPluginContainerImpl* GetWebPluginContainer() const;

  virtual void Trace(blink::Visitor*);

  virtual void TraceWrappers(const ScriptWrappableVisitor*) const;

 private:
  enum NodeFlags {
    kHasRareDataFlag = 1,

    // Node type flags. These never change once created.
    kIsTextFlag = 1 << 1,
    kIsContainerFlag = 1 << 2,
    kIsElementFlag = 1 << 3,
    kIsHTMLFlag = 1 << 4,
    kIsSVGFlag = 1 << 5,
    kIsDocumentFragmentFlag = 1 << 6,
    kIsV0InsertionPointFlag = 1 << 7,

    // Changes based on if the element should be treated like a link,
    // ex. When setting the href attribute on an <a>.
    kIsLinkFlag = 1 << 8,

    // Changes based on :hover, :active and :focus state.
    kIsUserActionElementFlag = 1 << 9,

    // Tree state flags. These change when the element is added/removed
    // from a DOM tree.
    kIsConnectedFlag = 1 << 10,
    kIsInShadowTreeFlag = 1 << 11,

    // Set by the parser when the children are done parsing.
    kIsFinishedParsingChildrenFlag = 1 << 12,

    // Flags related to recalcStyle.
    kHasCustomStyleCallbacksFlag = 1 << 13,
    kChildNeedsStyleInvalidationFlag = 1 << 14,
    kNeedsStyleInvalidationFlag = 1 << 15,
    kChildNeedsDistributionRecalcFlag = 1 << 16,
    kChildNeedsStyleRecalcFlag = 1 << 17,
    kStyleChangeMask =
        1 << kNodeStyleChangeShift | 1 << (kNodeStyleChangeShift + 1),

    kCustomElementStateMask = 0x3 << kNodeCustomElementShift,

    kHasNameOrIsEditingTextFlag = 1 << 22,
    kHasEventTargetDataFlag = 1 << 23,

    kV0CustomElementFlag = 1 << 24,
    kV0CustomElementUpgradedFlag = 1 << 25,

    kNeedsReattachLayoutTree = 1 << 26,
    kChildNeedsReattachLayoutTree = 1 << 27,

    kHasDuplicateAttributes = 1 << 28,

    kDefaultNodeFlags =
        kIsFinishedParsingChildrenFlag | kNeedsReattachStyleChange
  };

  // 4 bits remaining.

  bool GetFlag(NodeFlags mask) const { return node_flags_ & mask; }
  void SetFlag(bool f, NodeFlags mask) {
    node_flags_ = (node_flags_ & ~mask) | (-(int32_t)f & mask);
  }
  void SetFlag(NodeFlags mask) { node_flags_ |= mask; }
  void ClearFlag(NodeFlags mask) { node_flags_ &= ~mask; }

  // TODO(mustaq): This is a hack to fix sites with flash objects. We should
  // instead route all WebMouseEvents through EventHandler. See
  // crbug.com/665924.
  void CreateAndDispatchPointerEvent(const AtomicString& mouse_event_name,
                                     const WebMouseEvent&,
                                     LocalDOMWindow* view);

 protected:
  enum ConstructionType {
    kCreateOther = kDefaultNodeFlags,
    kCreateText = kDefaultNodeFlags | kIsTextFlag,
    kCreateContainer =
        kDefaultNodeFlags | kChildNeedsStyleRecalcFlag | kIsContainerFlag,
    kCreateElement = kCreateContainer | kIsElementFlag,
    kCreateShadowRoot =
        kCreateContainer | kIsDocumentFragmentFlag | kIsInShadowTreeFlag,
    kCreateDocumentFragment = kCreateContainer | kIsDocumentFragmentFlag,
    kCreateHTMLElement = kCreateElement | kIsHTMLFlag,
    kCreateSVGElement = kCreateElement | kIsSVGFlag,
    kCreateDocument = kCreateContainer | kIsConnectedFlag,
    kCreateV0InsertionPoint = kCreateHTMLElement | kIsV0InsertionPointFlag,
    kCreateEditingText = kCreateText | kHasNameOrIsEditingTextFlag,
    kCreatePseudoElement = kDefaultNodeFlags | kIsContainerFlag |
                           kIsElementFlag | kNeedsReattachLayoutTree,
  };

  Node(TreeScope*, ConstructionType);

  virtual void WillMoveToNewDocument(Document& old_document,
                                     Document& new_document);
  virtual void DidMoveToNewDocument(Document& old_document);

  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;
  void RemovedEventListener(const AtomicString& event_type,
                            const RegisteredEventListener&) override;
  DispatchEventResult DispatchEventInternal(Event*) override;

  bool HasRareData() const { return GetFlag(kHasRareDataFlag); }

  NodeRareData* RareData() const;
  NodeRareData& EnsureRareData();

  void SetHasCustomStyleCallbacks() {
    SetFlag(true, kHasCustomStyleCallbacksFlag);
  }

  void SetTreeScope(TreeScope* scope) { tree_scope_ = scope; }

  void MarkAncestorsWithChildNeedsStyleRecalc();

  void SetIsFinishedParsingChildren(bool value) {
    SetFlag(value, kIsFinishedParsingChildrenFlag);
  }

 private:
  // Gets nodeName without caching AtomicStrings. Used by
  // debugName. Compositor may call debugName from the "impl" thread
  // during "commit". The main thread is stopped at that time, but
  // it is not safe to cache AtomicStrings because those are
  // per-thread.
  virtual String DebugNodeName() const;

  bool IsUserActionElementActive() const;
  bool IsUserActionElementInActiveChain() const;
  bool IsUserActionElementDragged() const;
  bool IsUserActionElementHovered() const;
  bool IsUserActionElementFocused() const;
  bool IsUserActionElementHasFocusWithin() const;

  void RecalcDistribution();

  void SetStyleChange(StyleChangeType);

  virtual const ComputedStyle* VirtualEnsureComputedStyle(
      PseudoId = kPseudoIdNone);

  void TrackForDebugging();

  const HeapVector<TraceWrapperMember<MutationObserverRegistration>>*
  MutationObserverRegistry();
  const HeapHashSet<TraceWrapperMember<MutationObserverRegistration>>*
  TransientMutationObserverRegistry();

  uint32_t node_flags_;
  TraceWrapperMember<Node> parent_or_shadow_host_node_;
  Member<TreeScope> tree_scope_;
  TraceWrapperMember<Node> previous_;
  TraceWrapperMember<Node> next_;
  // When a node has rare data we move the layoutObject into the rare data.
  union DataUnion {
    DataUnion() : node_layout_data_(&NodeRenderingData::SharedEmptyData()) {}
    // LayoutObjects are fully owned by their DOM node. See LayoutObject's
    // LIFETIME documentation section.
    NodeRenderingData* node_layout_data_;
    NodeRareDataBase* rare_data_;
  } data_;
};

inline void Node::SetParentOrShadowHostNode(ContainerNode* parent) {
  DCHECK(IsMainThread());
  parent_or_shadow_host_node_ = reinterpret_cast<Node*>(parent);
}

inline ContainerNode* Node::ParentOrShadowHostNode() const {
  DCHECK(IsMainThread());
  return reinterpret_cast<ContainerNode*>(parent_or_shadow_host_node_.Get());
}

inline ContainerNode* Node::parentNode() const {
  return IsShadowRoot() ? nullptr : ParentOrShadowHostNode();
}

inline void Node::LazyReattachIfAttached() {
  if (NeedsAttach())
    return;
  if (!InActiveDocument())
    return;

  AttachContext context;
  context.performing_reattach = true;

  DetachLayoutTree(context);
  MarkAncestorsWithChildNeedsStyleRecalc();
}

inline bool Node::ShouldCallRecalcStyle(StyleRecalcChange change) {
  return change >= kIndependentInherit || NeedsStyleRecalc() ||
         ChildNeedsStyleRecalc();
}

// Allow equality comparisons of Nodes by reference or pointer, interchangeably.
DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(Node)

#define DEFINE_NODE_TYPE_CASTS(thisType, predicate) \
  DEFINE_TYPE_CASTS(thisType, Node, node, node->predicate, node.predicate)

// This requires isClassName(const Node&).
#define DEFINE_NODE_TYPE_CASTS_WITH_FUNCTION(thisType)         \
  DEFINE_TYPE_CASTS(thisType, Node, node, Is##thisType(*node), \
                    Is##thisType(node))

#define DECLARE_NODE_FACTORY(T) static T* Create(Document&)
#define DEFINE_NODE_FACTORY(T) \
  T* T::Create(Document& document) { return new T(document); }

CORE_EXPORT std::ostream& operator<<(std::ostream&, const Node&);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const Node*);

}  // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showNode(const blink::Node*);
void showTree(const blink::Node*);
void showNodePath(const blink::Node*);
#endif

#endif  // Node_h
