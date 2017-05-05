/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#include "core/html/HTMLFrameOwnerElement.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/page/Page.h"
#include "core/plugins/PluginView.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

typedef HeapHashMap<Member<FrameViewBase>, Member<FrameView>>
    FrameViewBaseToParentMap;
static FrameViewBaseToParentMap& WidgetNewParentMap() {
  DEFINE_STATIC_LOCAL(FrameViewBaseToParentMap, map,
                      (new FrameViewBaseToParentMap));
  return map;
}

using FrameViewBaseSet = HeapHashSet<Member<FrameViewBase>>;
static FrameViewBaseSet& WidgetsPendingTemporaryRemovalFromParent() {
  // FrameViewBases in this set will not leak because it will be cleared in
  // HTMLFrameOwnerElement::UpdateSuspendScope::performDeferredWidgetTreeOperations.
  DEFINE_STATIC_LOCAL(FrameViewBaseSet, set, (new FrameViewBaseSet));
  return set;
}

using FrameOrPluginList = HeapVector<Member<FrameOrPlugin>>;
static FrameOrPluginList& FrameOrPluginsPendingDispose() {
  DEFINE_STATIC_LOCAL(FrameOrPluginList, list, (new FrameOrPluginList));
  return list;
}

SubframeLoadingDisabler::SubtreeRootSet&
SubframeLoadingDisabler::DisabledSubtreeRoots() {
  DEFINE_STATIC_LOCAL(SubtreeRootSet, nodes, ());
  return nodes;
}

static unsigned g_update_suspend_count = 0;

HTMLFrameOwnerElement::UpdateSuspendScope::UpdateSuspendScope() {
  ++g_update_suspend_count;
}

void HTMLFrameOwnerElement::UpdateSuspendScope::
    PerformDeferredWidgetTreeOperations() {
  FrameViewBaseToParentMap map;
  WidgetNewParentMap().swap(map);
  for (const auto& frame_view_base : map) {
    FrameViewBase* child = frame_view_base.key.Get();
    FrameView* current_parent = ToFrameView(child->Parent());
    FrameView* new_parent = frame_view_base.value;
    if (new_parent != current_parent) {
      if (current_parent)
        current_parent->RemoveChild(child);
      if (new_parent)
        new_parent->AddChild(child);
      if (current_parent && !new_parent)
        child->Dispose();
    }
  }

  {
    FrameViewBaseSet set;
    WidgetsPendingTemporaryRemovalFromParent().swap(set);
    for (const auto& frame_view_base : set) {
      FrameView* current_parent = ToFrameView(frame_view_base->Parent());
      if (current_parent)
        current_parent->RemoveChild(frame_view_base.Get());
    }
  }

  {
    FrameOrPluginList list;
    FrameOrPluginsPendingDispose().swap(list);
    for (const auto& frame_or_plugin : list) {
      frame_or_plugin->Dispose();
    }
  }
}

HTMLFrameOwnerElement::UpdateSuspendScope::~UpdateSuspendScope() {
  DCHECK_GT(g_update_suspend_count, 0u);
  if (g_update_suspend_count == 1)
    PerformDeferredWidgetTreeOperations();
  --g_update_suspend_count;
}

// Unlike moveWidgetToParentSoon, this will not call dispose.
void TemporarilyRemoveWidgetFromParentSoon(FrameViewBase* frame_view_base) {
  if (g_update_suspend_count) {
    WidgetsPendingTemporaryRemovalFromParent().insert(frame_view_base);
  } else {
    if (ToFrameView(frame_view_base->Parent()))
      ToFrameView(frame_view_base->Parent())->RemoveChild(frame_view_base);
  }
}

void MoveWidgetToParentSoon(FrameViewBase* child, FrameView* parent) {
  if (!g_update_suspend_count) {
    if (parent) {
      parent->AddChild(child);
    } else if (ToFrameView(child->Parent())) {
      ToFrameView(child->Parent())->RemoveChild(child);
      child->Dispose();
    }
    return;
  }
  WidgetNewParentMap().Set(child, parent);
}

HTMLFrameOwnerElement::HTMLFrameOwnerElement(const QualifiedName& tag_name,
                                             Document& document)
    : HTMLElement(tag_name, document),
      content_frame_(nullptr),
      widget_(nullptr),
      sandbox_flags_(kSandboxNone) {}

LayoutPart* HTMLFrameOwnerElement::GetLayoutPart() const {
  // HTMLObjectElement and HTMLEmbedElement may return arbitrary layoutObjects
  // when using fallback content.
  if (!GetLayoutObject() || !GetLayoutObject()->IsLayoutPart())
    return nullptr;
  return ToLayoutPart(GetLayoutObject());
}

void HTMLFrameOwnerElement::SetContentFrame(Frame& frame) {
  // Make sure we will not end up with two frames referencing the same owner
  // element.
  DCHECK(!content_frame_ || content_frame_->Owner() != this);
  // Disconnected frames should not be allowed to load.
  DCHECK(isConnected());
  content_frame_ = &frame;

  for (ContainerNode* node = this; node; node = node->ParentOrShadowHostNode())
    node->IncrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::ClearContentFrame() {
  if (!content_frame_)
    return;

  DCHECK_EQ(content_frame_->Owner(), this);
  content_frame_ = nullptr;

  for (ContainerNode* node = this; node; node = node->ParentOrShadowHostNode())
    node->DecrementConnectedSubframeCount();

  GetDocument().CheckCompleted();
}

void HTMLFrameOwnerElement::DisconnectContentFrame() {
  // FIXME: Currently we don't do this in removedFrom because this causes an
  // unload event in the subframe which could execute script that could then
  // reach up into this document and then attempt to look back down. We should
  // see if this behavior is really needed as Gecko does not allow this.
  if (Frame* frame = ContentFrame()) {
    frame->Detach(FrameDetachType::kRemove);
  }
}

HTMLFrameOwnerElement::~HTMLFrameOwnerElement() {
  // An owner must by now have been informed of detachment
  // when the frame was closed.
  DCHECK(!content_frame_);
}

Document* HTMLFrameOwnerElement::contentDocument() const {
  return (content_frame_ && content_frame_->IsLocalFrame())
             ? ToLocalFrame(content_frame_)->GetDocument()
             : 0;
}

DOMWindow* HTMLFrameOwnerElement::contentWindow() const {
  return content_frame_ ? content_frame_->DomWindow() : 0;
}

void HTMLFrameOwnerElement::SetSandboxFlags(SandboxFlags flags) {
  sandbox_flags_ = flags;
  // Recalculate the container policy in case the allow-same-origin flag has
  // changed.
  container_policy_ = GetContainerPolicyFromAllowedFeatures(
      AllowedFeatures(), AllowFullscreen(), AllowPaymentRequest(),
      GetOriginForFeaturePolicy());

  // Don't notify about updates if ContentFrame() is null, for example when
  // the subframe hasn't been created yet.
  if (ContentFrame()) {
    GetDocument().GetFrame()->Loader().Client()->DidChangeFramePolicy(
        ContentFrame(), sandbox_flags_, container_policy_);
  }
}

bool HTMLFrameOwnerElement::IsKeyboardFocusable() const {
  return content_frame_ && HTMLElement::IsKeyboardFocusable();
}

void HTMLFrameOwnerElement::DisposeFrameOrPluginSoon(
    FrameOrPlugin* frame_or_plugin) {
  if (g_update_suspend_count) {
    FrameOrPluginsPendingDispose().push_back(frame_or_plugin);
    return;
  }
  frame_or_plugin->Dispose();
}

void HTMLFrameOwnerElement::UpdateContainerPolicy() {
  container_policy_ = GetContainerPolicyFromAllowedFeatures(
      AllowedFeatures(), AllowFullscreen(), AllowPaymentRequest(),
      GetOriginForFeaturePolicy());
  // Don't notify about updates if ContentFrame() is null, for example when
  // the subframe hasn't been created yet.
  if (ContentFrame()) {
    GetDocument().GetFrame()->Loader().Client()->DidChangeFramePolicy(
        ContentFrame(), sandbox_flags_, container_policy_);
  }
}

void HTMLFrameOwnerElement::FrameOwnerPropertiesChanged() {
  // Don't notify about updates if ContentFrame() is null, for example when
  // the subframe hasn't been created yet.
  if (ContentFrame()) {
    GetDocument().GetFrame()->Loader().Client()->DidChangeFrameOwnerProperties(
        this);
  }
}

void HTMLFrameOwnerElement::DispatchLoad() {
  DispatchScopedEvent(Event::Create(EventTypeNames::load));
}

const WebVector<WebFeaturePolicyFeature>&
HTMLFrameOwnerElement::AllowedFeatures() const {
  DEFINE_STATIC_LOCAL(WebVector<WebFeaturePolicyFeature>, features, ());
  return features;
}

const WebParsedFeaturePolicy& HTMLFrameOwnerElement::ContainerPolicy() const {
  return container_policy_;
}

Document* HTMLFrameOwnerElement::getSVGDocument(
    ExceptionState& exception_state) const {
  Document* doc = contentDocument();
  if (doc && doc->IsSVGDocument())
    return doc;
  return nullptr;
}

void HTMLFrameOwnerElement::SetWidget(FrameViewBase* frame_view_base) {
  if (frame_view_base == widget_)
    return;

  Document* doc = contentDocument();
  if (doc && doc->GetFrame()) {
    bool will_be_display_none = !frame_view_base;
    if (IsDisplayNone() != will_be_display_none) {
      doc->WillChangeFrameOwnerProperties(
          MarginWidth(), MarginHeight(), ScrollingMode(), will_be_display_none);
    }
  }

  if (widget_) {
    if (widget_->Parent())
      MoveWidgetToParentSoon(widget_.Get(), 0);
    widget_ = nullptr;
  }

  widget_ = frame_view_base;
  FrameOwnerPropertiesChanged();

  LayoutPart* layout_part = ToLayoutPart(GetLayoutObject());
  LayoutPartItem layout_part_item = LayoutPartItem(layout_part);
  if (layout_part_item.IsNull())
    return;

  if (widget_) {
    layout_part_item.UpdateOnWidgetChange();

    DCHECK_EQ(GetDocument().View(), layout_part_item.GetFrameView());
    DCHECK(layout_part_item.GetFrameView());
    MoveWidgetToParentSoon(widget_.Get(), layout_part_item.GetFrameView());
  }

  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->ChildrenChanged(layout_part);
}

FrameViewBase* HTMLFrameOwnerElement::ReleaseWidget() {
  if (!widget_)
    return nullptr;
  if (widget_->Parent())
    TemporarilyRemoveWidgetFromParentSoon(widget_.Get());
  LayoutPart* layout_part = ToLayoutPart(GetLayoutObject());
  if (layout_part) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
      cache->ChildrenChanged(layout_part);
  }
  return widget_.Release();
}

FrameViewBase* HTMLFrameOwnerElement::OwnedWidget() const {
  return widget_.Get();
}

bool HTMLFrameOwnerElement::LoadOrRedirectSubframe(
    const KURL& url,
    const AtomicString& frame_name,
    bool replace_current_item) {
  UpdateContainerPolicy();

  LocalFrame* parent_frame = GetDocument().GetFrame();
  if (ContentFrame()) {
    ContentFrame()->Navigate(GetDocument(), url, replace_current_item,
                             UserGestureStatus::kNone);
    return true;
  }

  if (!GetDocument().GetSecurityOrigin()->CanDisplay(url)) {
    FrameLoader::ReportLocalLoadFailed(parent_frame, url.GetString());
    return false;
  }

  if (!SubframeLoadingDisabler::CanLoadFrame(*this))
    return false;

  if (GetDocument().GetFrame()->GetPage()->SubframeCount() >=
      Page::kMaxNumberOfFrames)
    return false;

  FrameLoadRequest frame_load_request(&GetDocument(), ResourceRequest(url),
                                      "_self", kCheckContentSecurityPolicy);

  ReferrerPolicy policy = ReferrerPolicyAttribute();
  if (policy != kReferrerPolicyDefault)
    frame_load_request.GetResourceRequest().SetHTTPReferrer(
        SecurityPolicy::GenerateReferrer(policy, url,
                                         GetDocument().OutgoingReferrer()));

  return parent_frame->Loader().Client()->CreateFrame(frame_load_request,
                                                      frame_name, this);
}

DEFINE_TRACE(HTMLFrameOwnerElement) {
  visitor->Trace(content_frame_);
  visitor->Trace(widget_);
  HTMLElement::Trace(visitor);
  FrameOwner::Trace(visitor);
}

}  // namespace blink
