/*
 * Copyright (C) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_CACHE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_CACHE_IMPL_H_

#include <memory>
#include <utility>

#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache_base.h"
#include "third_party/blink/renderer/core/accessibility/blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/accessibility/ax_enums.mojom-blink-forward.h"
#include "ui/accessibility/ax_mode.h"

namespace blink {

class AbstractInlineTextBox;
class AXRelationCache;
class HTMLAreaElement;
class LocalFrameView;

// This class should only be used from inside the accessibility directory.
class MODULES_EXPORT AXObjectCacheImpl
    : public AXObjectCacheBase,
      public mojom::blink::PermissionObserver {
 public:
  static AXObjectCache* Create(Document&, const ui::AXMode&);

  AXObjectCacheImpl(Document&, const ui::AXMode&);
  ~AXObjectCacheImpl() override;
  void Trace(Visitor*) const override;

  Document& GetDocument() const { return *document_; }
  AXObject* FocusedObject();

  const ui::AXMode& GetAXMode() override;
  void SetAXMode(const ui::AXMode&) override;

  void Dispose() override;

  void Freeze() override { is_frozen_ = true; }
  void Thaw() override { is_frozen_ = false; }
  bool IsFrozen() { return is_frozen_; }

  //
  // Iterators.
  //

  void SelectionChanged(Node*) override;
  void UpdateReverseRelations(const AXObject* relation_source,
                              const Vector<String>& target_ids);
  void ChildrenChanged(AXObject*);
  void ChildrenChangedWithCleanLayout(AXObject*);
  void ChildrenChanged(Node*) override;
  void ChildrenChanged(const LayoutObject*) override;
  void ChildrenChanged(AccessibleNode*) override;
  void SlotAssignmentWillChange(Node*) override;
  void CheckedStateChanged(Node*) override;
  void ListboxOptionStateChanged(HTMLOptionElement*) override;
  void ListboxSelectedChildrenChanged(HTMLSelectElement*) override;
  void ListboxActiveIndexChanged(HTMLSelectElement*) override;
  void LocationChanged(const LayoutObject*) override;
  void ImageLoaded(const LayoutObject*) override;

  void Remove(AccessibleNode*) override;
  // Returns false if no associated AXObject exists in the cache.
  bool Remove(LayoutObject*) override;
  void Remove(Node*) override;
  void Remove(AbstractInlineTextBox*) override;
  void Remove(AXObject*);  // Calls more specific Remove methods as necessary.
  // For any ancestor that could contain the passed-in AXObject* in their cached
  // children, clear their children and set needs to update children on them.
  // In addition, ChildrenChanged() on an included ancestor that might contain
  // this child, if one exists.
  void ChildrenChangedOnAncestorOf(AXObject*);

  const Element* RootAXEditableElement(const Node*) override;

  // Called when aspects of the style (e.g. color, alignment) change.
  void StyleChanged(const LayoutObject*) override;

  // Called by a node when text or a text equivalent (e.g. alt) attribute is
  // changed.
  void TextChanged(const LayoutObject*) override;
  void TextChangedWithCleanLayout(Node* optional_node, AXObject*);
  void FocusableChangedWithCleanLayout(Element* element);
  void DocumentTitleChanged() override;
  // Called when a layout tree for a node has just been attached, so we can make
  // sure we have the right subclass of AXObject.
  void UpdateCacheAfterNodeIsAttached(Node*) override;
  // A DOM node was inserted , but does not necessarily have a layout tree.
  void DidInsertChildrenOfNode(Node*) override;

  void HandleAttributeChanged(const QualifiedName& attr_name,
                              Element*) override;
  void HandleValidationMessageVisibilityChanged(
      const Node* form_control) override;
  void HandleEventListenerAdded(const Node& node,
                                const AtomicString& event_type) override;
  void HandleEventListenerRemoved(const Node& node,
                                  const AtomicString& event_type) override;
  void HandleFocusedUIElementChanged(Element* old_focused_element,
                                     Element* new_focused_element) override;
  void HandleInitialFocus() override;
  void HandleTextFormControlChanged(Node*) override;
  void HandleEditableTextContentChanged(Node*) override;
  void HandleScaleAndLocationChanged(Document*) override;
  void HandleTextMarkerDataAdded(Node* start, Node* end) override;
  void HandleValueChanged(Node*) override;
  void HandleUpdateActiveMenuOption(Node*) override;
  void DidShowMenuListPopup(LayoutObject*) override;
  void DidHideMenuListPopup(LayoutObject*) override;
  void HandleLoadComplete(Document*) override;
  void HandleLayoutComplete(Document*) override;
  void HandleClicked(Node*) override;
  void HandleAttributeChanged(const QualifiedName& attr_name,
                              AccessibleNode*) override;

  void SetCanvasObjectBounds(HTMLCanvasElement*,
                             Element*,
                             const LayoutRect&) override;

  void InlineTextBoxesUpdated(LayoutObject*) override;
  void ProcessDeferredAccessibilityEvents(Document&) override;
  // Is there work to be done when layout becomes clean?
  bool IsDirty() const override;

  // Called when a HTMLFrameOwnerElement (such as an iframe element) changes the
  // embedding token of its child frame.
  void EmbeddingTokenChanged(HTMLFrameOwnerElement*) override;

  // Called when the scroll offset changes.
  void HandleScrollPositionChanged(LocalFrameView*) override;
  void HandleScrollPositionChanged(LayoutObject*) override;

  void HandleScrolledToAnchor(const Node* anchor_node) override;

  // Called when the frame rect changes, which can sometimes happen
  // without producing any layout or other notifications.
  void HandleFrameRectsChanged(Document&) override;

  // Invalidates the bounding box, which can be later retrieved by
  // GetAllObjectsWithChangedBounds.
  void InvalidateBoundingBox(const LayoutObject*) override;

  const AtomicString& ComputedRoleForNode(Node*) override;
  String ComputedNameForNode(Node*) override;

  void OnTouchAccessibilityHover(const IntPoint&) override;

  AXObject* ObjectFromAXID(AXID id) const {
    auto it = objects_.find(id);
    return it != objects_.end() ? it->value : nullptr;
  }
  AXObject* Root();

  // Used for objects without backing DOM nodes, layout objects, etc.
  AXObject* CreateAndInit(ax::mojom::blink::Role, AXObject* parent);

  AXObject* GetOrCreate(AccessibleNode*, AXObject* parent);
  AXObject* GetOrCreate(LayoutObject*, AXObject* parent_if_known) override;
  AXObject* GetOrCreate(LayoutObject* layout_object);
  AXObject* GetOrCreate(const Node*, AXObject* parent_if_known);
  AXObject* GetOrCreate(Node*, AXObject* parent_if_known);
  AXObject* GetOrCreate(Node*);
  AXObject* GetOrCreate(const Node*);
  AXObject* GetOrCreate(AbstractInlineTextBox*, AXObject* parent_if_known);

  AXID GetAXID(Node*) override;
  Element* GetElementFromAXID(AXID) override;

  AXObject* Get(AccessibleNode*);
  AXObject* Get(AbstractInlineTextBox*);

  // Get an AXObject* backed by the passed-in DOM node or the node's layout
  // object, whichever is available.
  // If it no longer the correct type of AXObject (AXNodeObject/AXLayoutObject),
  // will Invalidate() the AXObject so that it is refreshed with a new object
  // when safe to do so.
  AXObject* Get(const Node*);
  AXObject* Get(const LayoutObject*);

  // Get an AXObject* without any potential side effects, such as queuing up an
  // invalidation for objects that need to be removed or replaced.
  AXObject* GetWithoutInvalidation(const Node* node);

  // Return true if the object is still part of the tree, meaning that ancestors
  // exist or can be repaired all the way to the root.
  bool IsStillInTree(AXObject*);

  void ChildrenChangedWithCleanLayout(Node* optional_node_for_relation_update,
                                      AXObject*);

  void MarkAXObjectDirtyWithCleanLayout(AXObject*);
  void MarkAXSubtreeDirtyWithCleanLayout(AXObject*);

  // Set the parent of |child|. If no parent is possible, this means the child
  // can no longer be in the AXTree, so remove the child.
  AXObject* RestoreParentOrPrune(AXObject* child);

  // When an object is created or its id changes, this must be called so that
  // the relation cache is updated.
  void MaybeNewRelationTarget(Node& node, AXObject* obj);

  void HandleActiveDescendantChangedWithCleanLayout(Node*);
  void SectionOrRegionRoleMaybeChanged(Element* element);
  void HandleRoleChangeWithCleanLayout(Node*);
  void HandleAriaHiddenChangedWithCleanLayout(Node*);
  void HandleAriaExpandedChangeWithCleanLayout(Node*);
  void HandleAriaSelectedChangedWithCleanLayout(Node*);
  void HandleNodeLostFocusWithCleanLayout(Node*);
  void HandleNodeGainedFocusWithCleanLayout(Node*);
  void HandleLoadCompleteWithCleanLayout(Node*);
  void UpdateCacheAfterNodeIsAttachedWithCleanLayout(Node*);
  void DidShowMenuListPopupWithCleanLayout(Node*);
  void DidHideMenuListPopupWithCleanLayout(Node*);
  void StyleChangedWithCleanLayout(Node*);
  void HandleScrollPositionChangedWithCleanLayout(Node*);
  void HandleValidationMessageVisibilityChangedWithCleanLayout(const Node*);
  void HandleUpdateActiveMenuOptionWithCleanLayout(Node*);
  void HandleEditableTextContentChangedWithCleanLayout(Node*);

  bool InlineTextBoxAccessibilityEnabled();

  void RemoveAXID(AXObject*);

  AXID GenerateAXID() const;

  // Counts the number of times the document has been modified. Some attribute
  // values are cached as long as the modification count hasn't changed.
  int ModificationCount() const { return modification_count_; }
  void IncrementModificationCount() { ++modification_count_; }

  void PostNotification(const LayoutObject*, ax::mojom::blink::Event);
  // Creates object if necessary.
  void EnsurePostNotification(Node*, ax::mojom::blink::Event);
  // Does not create object.
  // TODO(accessibility) Find out if we can merge with EnsurePostNotification().
  void PostNotification(Node*, ax::mojom::blink::Event);
  void PostNotification(AXObject*, ax::mojom::blink::Event);

  //
  // Aria-owns support.
  //

  // Returns true if the given object's position in the tree was due to
  // aria-owns.
  bool IsAriaOwned(const AXObject*) const;

  // Returns the parent of the given object due to aria-owns, if valid.
  AXObject* ValidatedAriaOwner(const AXObject*) const;

  // Given an object that has an aria-owns attribute, return the validated
  // set of aria-owned children.
  void ValidatedAriaOwnedChildren(const AXObject* owner,
                                  HeapVector<Member<AXObject>>& owned_children);

  // Given a <map> element, get the image currently associated with it, if any.
  AXObject* GetAXImageForMap(HTMLMapElement& map);

  // Adds |object| to |fixed_or_sticky_node_ids_| if it has a fixed or sticky
  // position.
  void AddToFixedOrStickyNodeList(const AXObject* object);

  bool MayHaveHTMLLabel(const HTMLElement& elem);

  // Synchronously returns whether or not we currently have permission to
  // call AOM event listeners.
  bool CanCallAOMEventListeners() const;

  // This is called when an accessibility event is triggered and there are
  // AOM event listeners registered that would have been called.
  // Asynchronously requests permission from the user. If permission is
  // granted, it only applies to the next event received.
  void RequestAOMEventListenerPermission();

  // For built-in HTML form validation messages. Set notify_children_changed to
  // true if not already processing changed children.
  AXObject* ValidationMessageObjectIfInvalid(bool notify_children_changed);

  WebAXAutofillState GetAutofillState(AXID id) const;
  void SetAutofillState(AXID id, WebAXAutofillState state);

  std::pair<ax::mojom::blink::EventFrom, ax::mojom::blink::Action>
  active_event_from_data() const {
    return std::make_pair(active_event_from_, active_event_from_action_);
  }

  void set_active_event_from_data(
      const ax::mojom::blink::EventFrom event_from,
      const ax::mojom::blink::Action event_from_action) {
    active_event_from_ = event_from;
    active_event_from_action_ = event_from_action;
  }

  AXObject* GetActiveAriaModalDialog() const;

  static bool UseAXMenuList() { return use_ax_menu_list_; }
  static bool ShouldCreateAXMenuListFor(LayoutObject* layout_object);
  static bool ShouldCreateAXMenuListOptionFor(const Node*);
  static bool IsRelevantPseudoElement(const Node& node);
  static bool IsRelevantPseudoElementDescendant(
      const LayoutObject& layout_object);

#if DCHECK_IS_ON()
  bool HasBeenDisposed() { return has_been_disposed_; }
#endif

  // Retrieves a vector of all AXObjects whose bounding boxes may have changed
  // since the last query. Clears the vector so that the next time it's
  // called, it will only retrieve objects that have changed since now.
  HeapVector<Member<AXObject>> GetAllObjectsWithChangedBounds();

 protected:
  void PostPlatformNotification(
      AXObject* obj,
      ax::mojom::blink::Event event_type,
      ax::mojom::blink::EventFrom event_from =
          ax::mojom::blink::EventFrom::kNone,
      ax::mojom::blink::Action event_from_action =
          ax::mojom::blink::Action::kNone,
      const BlinkAXEventIntentsSet& event_intents = BlinkAXEventIntentsSet());
  void LabelChangedWithCleanLayout(Element*);

  // Returns a reference to the set of currently active event intents.
  BlinkAXEventIntentsSet& ActiveEventIntents() override {
    return active_event_intents_;
  }

  // Create an AXObject, and do not check if a previous one exists.
  // Also, initialize the object and add it to maps for later retrieval.
  AXObject* CreateAndInit(Node*, AXObject* parent_if_known, AXID use_axid = 0);
  AXObject* CreateAndInit(LayoutObject*,
                          AXObject* parent_if_known,
                          AXID use_axid = 0);

  // Mark object as invalid and needing to be refreshed when layout is clean.
  // Will result in a new object with the same AXID, and will also call
  // ChildrenChanged() on the parent of invalidated objects. Automatically
  // de-dupes extra object refreshes and ChildrenChanged() calls.
  void Invalidate(Document&, AXID);

  AXObject* CreateFromRenderer(LayoutObject*);
  AXObject* CreateFromNode(Node*);
  AXObject* CreateFromInlineTextBox(AbstractInlineTextBox*);
  void Remove(AXID);

 private:
  struct AXEventParams final : public GarbageCollected<AXEventParams> {
    AXEventParams(AXObject* target,
                  ax::mojom::blink::Event event_type,
                  ax::mojom::blink::EventFrom event_from,
                  ax::mojom::blink::Action event_from_action,
                  const BlinkAXEventIntentsSet& intents)
        : target(target),
          event_type(event_type),
          event_from(event_from),
          event_from_action(event_from_action) {
      for (const auto& intent : intents) {
        event_intents.insert(intent.key, intent.value);
      }
    }
    Member<AXObject> target;
    ax::mojom::blink::Event event_type;
    ax::mojom::blink::EventFrom event_from;
    ax::mojom::blink::Action event_from_action;
    BlinkAXEventIntentsSet event_intents;

    void Trace(Visitor* visitor) const { visitor->Trace(target); }
  };

  struct TreeUpdateParams final : public GarbageCollected<TreeUpdateParams> {
    TreeUpdateParams(const Node* node,
                     AXID axid,
                     ax::mojom::blink::EventFrom event_from,
                     ax::mojom::blink::Action event_from_action,
                     const BlinkAXEventIntentsSet& intents,
                     base::OnceClosure callback)
        : node(node),
          axid(axid),
          event_from(event_from),
          event_from_action(event_from_action),
          callback(std::move(callback)) {
      for (const auto& intent : intents) {
        event_intents.insert(intent.key, intent.value);
      }
    }
    WeakMember<const Node> node;
    AXID axid;
    ax::mojom::blink::EventFrom event_from;
    ax::mojom::blink::Action event_from_action;
    BlinkAXEventIntentsSet event_intents;
    base::OnceClosure callback;

    void Trace(Visitor* visitor) const { visitor->Trace(node); }
  };
  typedef HeapVector<Member<TreeUpdateParams>> TreeUpdateCallbackQueue;

  ax::mojom::blink::EventFrom ComputeEventFrom();

  void MarkAXObjectDirtyWithCleanLayoutHelper(AXObject* obj, bool subtree);
  void MarkAXObjectDirty(AXObject*);
  void MarkAXSubtreeDirty(AXObject*);
  void MarkElementDirty(const Node*);
  void MarkElementDirtyWithCleanLayout(const Node*);

  // Given an object to mark dirty or fire an event on, return an object
  // included in the tree that can be used with the serializer, or null if there
  // is no relevant object to use. Objects that are not included in the tree,
  // and have no ancestor object included in the tree, are pruned from the tree,
  // in which case there is nothing to be serialized.
  AXObject* GetSerializationTarget(AXObject* obj);

  // Helper that clears children up to the first included ancestor and returns
  // the ancestor if a children changed notification should be fired on it.
  AXObject* InvalidateChildren(AXObject* obj);

  Member<Document> document_;
  ui::AXMode ax_mode_;
  HeapHashMap<AXID, Member<AXObject>> objects_;
  // LayoutObject and AbstractInlineTextBox are not on the Oilpan heap so we
  // do not use HeapHashMap for those mappings.
  HeapHashMap<Member<AccessibleNode>, AXID> accessible_node_mapping_;
  HashMap<const LayoutObject*, AXID> layout_object_mapping_;
  HeapHashMap<Member<const Node>, AXID> node_object_mapping_;
  HashMap<AbstractInlineTextBox*, AXID> inline_text_box_object_mapping_;
  int modification_count_;

  HashSet<AXID> ids_in_use_;

  // Used for a mock AXObject representing the message displayed in the
  // validation message bubble.
  // There can be only one of these per document with invalid form controls,
  // and it will always be related to the currently focused control.
  AXID validation_message_axid_;

  // The currently active aria-modal dialog element, if one has been computed,
  // null if otherwise. This is only ever computed on platforms that have the
  // AriaModalPrunesAXTree setting enabled, such as Mac.
  WeakMember<AXObject> active_aria_modal_dialog_;

  std::unique_ptr<AXRelationCache> relation_cache_;

#if DCHECK_IS_ON()
  // Verified when finalizing.
  bool has_been_disposed_ = false;
#endif

  HeapVector<Member<AXEventParams>> notifications_to_post_main_;
  HeapVector<Member<AXEventParams>> notifications_to_post_popup_;

  // Call the queued callback methods that do processing which must occur when
  // layout is clean. These callbacks are stored in tree_update_callback_queue_,
  // and have names like FooBarredWithCleanLayout().
  void ProcessCleanLayoutCallbacks(Document&);

  // Destroy and recreate any objects which are no longer valid, for example
  // they used to be an AXNodeObject and now must be an AXLayoutObject, or
  // vice-versa. Also fires children changed on the parent of these nodes.
  void ProcessInvalidatedObjects(Document&);

  // Send events to RenderAccessibilityImpl, which serializes them and then
  // sends the serialized events and dirty objects to the browser process.
  void PostNotifications(Document&);

  // Get the currently focused Node element.
  Node* FocusedElement();

  // GetOrCreate the focusable AXObject for a specific Node.
  AXObject* GetOrCreateFocusedObjectFromNode(Node*);

  AXObject* FocusedImageMapUIElement(HTMLAreaElement*);

  // Associate an AXObject with an AXID. Generate one if none is supplied.
  AXID AssociateAXID(AXObject*, AXID use_axid = 0);

  void TextChanged(Node*);
  bool NodeIsTextControl(const Node*);
  AXObject* NearestExistingAncestor(Node*);

  Settings* GetSettings();

  // Start listenening for updates to the AOM accessibility event permission.
  void AddPermissionStatusListener();

  // mojom::blink::PermissionObserver implementation.
  // Called when we get an updated AOM event listener permission value from
  // the browser.
  void OnPermissionStatusChange(mojom::PermissionStatus) override;

  // When a <tr> or <td> is inserted or removed, the containing table may have
  // gained or lost rows or columns.
  void ContainingTableRowsOrColsMaybeChanged(Node*);

  // Must be called an entire subtree of accessible objects are no longer valid.
  void RemoveAXObjectsInLayoutSubtree(AXObject* subtree, int depth);

  // Object for HTML validation alerts. Created at most once per object cache.
  AXObject* GetOrCreateValidationMessageObject();
  void RemoveValidationMessageObjectWithCleanLayout(Node* document);

  // Enqueue a callback to the given method to be run after layout is
  // complete.
  void DeferTreeUpdate(void (AXObjectCacheImpl::*method)(const Node*),
                       const Node* node);
  void DeferTreeUpdate(void (AXObjectCacheImpl::*method)(Node*), Node* node);
  void DeferTreeUpdate(
      void (AXObjectCacheImpl::*method)(Node* node,
                                        ax::mojom::blink::Event event),
      Node* node,
      ax::mojom::blink::Event event);
  void DeferTreeUpdate(void (AXObjectCacheImpl::*method)(const QualifiedName&,
                                                         Element* element),
                       const QualifiedName& attr_name,
                       Element* element);
  // Provide either a DOM node or AXObject. If both are provided, then they must
  // match, meaning that the AXObject's DOM node must equal the provided node.
  void DeferTreeUpdate(void (AXObjectCacheImpl::*method)(Node*, AXObject*),
                       AXObject* obj);

  void DeferTreeUpdateInternal(base::OnceClosure callback, const Node* node);
  void DeferTreeUpdateInternal(base::OnceClosure callback, AXObject* obj);

  void SelectionChangedWithCleanLayout(Node* node);
  void TextChangedWithCleanLayout(Node* node);
  void ChildrenChangedWithCleanLayout(Node* node);
  void HandleAttributeChangedWithCleanLayout(const QualifiedName& attr_name,
                                             Element* element);
  void HandleUseMapAttributeChangedWithCleanLayout(Element*);
  void HandleNameAttributeChangedWithCleanLayout(Element*);

  bool DoesEventListenerImpactIgnoredState(
      const AtomicString& event_type) const;
  void HandleEventSubscriptionChanged(const Node& node,
                                      const AtomicString& event_type);

  //
  // aria-modal support
  //

  // This function is only ever called on platforms where the
  // AriaModalPrunesAXTree setting is enabled, and the accessibility tree must
  // be manually pruned to remove background content.
  void UpdateActiveAriaModalDialog(Node* element);

  // This will return null on platforms without the AriaModalPrunesAXTree
  // setting enabled, or where there is no active ancestral aria-modal dialog.
  AXObject* AncestorAriaModalDialog(Node* node);

  void ScheduleVisualUpdate(Document& document);
  void FireTreeUpdatedEventImmediately(
      Document& document,
      ax::mojom::blink::EventFrom event_from,
      ax::mojom::blink::Action event_from_action,
      const BlinkAXEventIntentsSet& event_intents,
      base::OnceClosure callback);
  void FireAXEventImmediately(AXObject* obj,
                              ax::mojom::blink::Event event_type,
                              ax::mojom::blink::EventFrom event_from,
                              ax::mojom::blink::Action event_from_action,
                              const BlinkAXEventIntentsSet& event_intents);

  void SetMaxPendingUpdatesForTesting(wtf_size_t max_pending_updates) {
    max_pending_updates_ = max_pending_updates;
  }

  void UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram();

  // Invalidates the bounding boxes of fixed or sticky positioned objects which
  // should be updated when the scroll offset is changed. Like
  // InvalidateBoundingBox, it can be later retrieved by
  // GetAllObjectsWithChangedBounds.
  void InvalidateBoundingBoxForFixedOrStickyPosition();

  // Return true if this is the popup document. There can only be one popup
  // document at a time. If it is not the popup document, it's the main
  // document stored in |document_|.
  bool IsPopup(Document& document) const;

  // Get the invalidated objects for the passed-in document.
  HashSet<AXID>& GetInvalidatedIds(Document& document);

  // Get the queued tree update callbacks for the passed-in document
  TreeUpdateCallbackQueue& GetTreeUpdateCallbackQueue(Document& document);

  // Get the event notifications to post for the passed-in document.
  HeapVector<Member<AXEventParams>>& GetNotificationsToPost(Document& document);

  // Whether the user has granted permission for the user to install event
  // listeners for accessibility events using the AOM.
  mojom::PermissionStatus accessibility_event_permission_;
  // The permission service, enabling us to check for event listener
  // permission.
  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;
  HeapMojoReceiver<mojom::blink::PermissionObserver, AXObjectCacheImpl>
      permission_observer_receiver_;

  // Queued callbacks.
  TreeUpdateCallbackQueue tree_update_callback_queue_main_;
  TreeUpdateCallbackQueue tree_update_callback_queue_popup_;

  HeapHashSet<WeakMember<Node>> nodes_with_pending_children_changed_;

  // If tree_update_callback_queue_ gets improbably large, stop
  // enqueueing updates and fire a single ChildrenChanged event on the
  // document once layout occurs.
  wtf_size_t max_pending_updates_ = 1UL << 16;
  bool tree_updates_paused_ = false;

  // Maps ids to their object's autofill state.
  HashMap<AXID, WebAXAutofillState> autofill_state_map_;

  // The set of node IDs whose bounds has changed since the last time
  // GetAllObjectsWithChangedBounds was called.
  HashSet<AXID> changed_bounds_ids_;

  // The list of node IDs whose position is fixed or sticky.
  HashSet<AXID> fixed_or_sticky_node_ids_;

  // The source of the event that is currently being handled.
  ax::mojom::blink::EventFrom active_event_from_ =
      ax::mojom::blink::EventFrom::kNone;

  // The accessibility action that caused the event. Will only be valid if
  // active_event_from_ is set to kAction.
  ax::mojom::blink::Action active_event_from_action_ =
      ax::mojom::blink::Action::kNone;

  // A set of currently active event intents.
  BlinkAXEventIntentsSet active_event_intents_;

  bool is_frozen_ = false;  // Used with Freeze(), Thaw() and IsFrozen() above.

  // Set of ID's of current AXObjects that need to be destroyed and recreated.
  HashSet<AXID> invalidated_ids_main_;
  HashSet<AXID> invalidated_ids_popup_;

  // If false, exposes the internal accessibility tree of a select pop-up
  // instead.
  static bool use_ax_menu_list_;

  DISALLOW_COPY_AND_ASSIGN(AXObjectCacheImpl);

  FRIEND_TEST_ALL_PREFIXES(AccessibilityTest, PauseUpdatesAfterMaxNumberQueued);
};

// This is the only subclass of AXObjectCache.
template <>
struct DowncastTraits<AXObjectCacheImpl> {
  static bool AllowFrom(const AXObjectCache& cache) { return true; }
};

// This will let you know if aria-hidden was explicitly set to false.
bool IsNodeAriaVisible(Node*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_CACHE_IMPL_H_
