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
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/RemoteFrameView.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/api/LayoutEmbeddedContentItem.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/page/Page.h"
#include "core/plugins/PluginView.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebCachePolicy.h"

namespace blink {

using PluginSet = HeapHashSet<Member<PluginView>>;
static PluginSet& PluginsPendingDispose() {
  DEFINE_STATIC_LOCAL(PluginSet, set, (new PluginSet));
  return set;
}

SubframeLoadingDisabler::SubtreeRootSet&
SubframeLoadingDisabler::DisabledSubtreeRoots() {
  DEFINE_STATIC_LOCAL(SubtreeRootSet, nodes, ());
  return nodes;
}

static unsigned g_plugin_dispose_suspend_count = 0;

HTMLFrameOwnerElement::PluginDisposeSuspendScope::PluginDisposeSuspendScope() {
  ++g_plugin_dispose_suspend_count;
}

void HTMLFrameOwnerElement::PluginDisposeSuspendScope::
    PerformDeferredPluginDispose() {
  PluginSet dispose_set;
  PluginsPendingDispose().swap(dispose_set);
  for (const auto& plugin : dispose_set) {
    plugin->Dispose();
  }
}

HTMLFrameOwnerElement::PluginDisposeSuspendScope::~PluginDisposeSuspendScope() {
  DCHECK_GT(g_plugin_dispose_suspend_count, 0u);
  if (g_plugin_dispose_suspend_count == 1)
    PerformDeferredPluginDispose();
  --g_plugin_dispose_suspend_count;
}

HTMLFrameOwnerElement::HTMLFrameOwnerElement(const QualifiedName& tag_name,
                                             Document& document)
    : HTMLElement(tag_name, document),
      content_frame_(nullptr),
      embedded_content_view_(nullptr),
      sandbox_flags_(kSandboxNone) {}

LayoutEmbeddedContent* HTMLFrameOwnerElement::GetLayoutEmbeddedContent() const {
  // HTMLObjectElement and HTMLEmbedElement may return arbitrary layoutObjects
  // when using fallback content.
  if (!GetLayoutObject() || !GetLayoutObject()->IsLayoutEmbeddedContent())
    return nullptr;
  return ToLayoutEmbeddedContent(GetLayoutObject());
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
  container_policy_ = ConstructContainerPolicy();

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

void HTMLFrameOwnerElement::DisposePluginSoon(PluginView* plugin) {
  if (g_plugin_dispose_suspend_count)
    PluginsPendingDispose().insert(plugin);
  else
    plugin->Dispose();
}

void HTMLFrameOwnerElement::UpdateContainerPolicy() {
  container_policy_ = ConstructContainerPolicy();
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

void HTMLFrameOwnerElement::SetEmbeddedContentView(
    EmbeddedContentView* embedded_content_view) {
  if (embedded_content_view == embedded_content_view_)
    return;

  Document* doc = contentDocument();
  if (doc && doc->GetFrame()) {
    bool will_be_display_none = !embedded_content_view;
    if (IsDisplayNone() != will_be_display_none) {
      doc->WillChangeFrameOwnerProperties(
          MarginWidth(), MarginHeight(), ScrollingMode(), will_be_display_none);
    }
  }

  if (embedded_content_view_) {
    if (embedded_content_view_->IsAttached()) {
      embedded_content_view_->DetachFromLayout();
      if (embedded_content_view_->IsPluginView())
        DisposePluginSoon(ToPluginView(embedded_content_view_));
      else
        embedded_content_view_->Dispose();
    }
  }

  embedded_content_view_ = embedded_content_view;
  FrameOwnerPropertiesChanged();

  LayoutEmbeddedContent* layout_embedded_content =
      ToLayoutEmbeddedContent(GetLayoutObject());
  LayoutEmbeddedContentItem layout_embedded_content_item =
      LayoutEmbeddedContentItem(layout_embedded_content);
  if (layout_embedded_content_item.IsNull())
    return;

  if (embedded_content_view_) {
    layout_embedded_content_item.UpdateOnEmbeddedContentViewChange();

    DCHECK_EQ(GetDocument().View(),
              layout_embedded_content_item.GetFrameView());
    DCHECK(layout_embedded_content_item.GetFrameView());
    embedded_content_view_->AttachToLayout();
  }

  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->ChildrenChanged(layout_embedded_content);
}

EmbeddedContentView* HTMLFrameOwnerElement::ReleaseEmbeddedContentView() {
  if (!embedded_content_view_)
    return nullptr;
  if (embedded_content_view_->IsAttached())
    embedded_content_view_->DetachFromLayout();
  LayoutEmbeddedContent* layout_embedded_content =
      ToLayoutEmbeddedContent(GetLayoutObject());
  if (layout_embedded_content) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
      cache->ChildrenChanged(layout_embedded_content);
  }
  return embedded_content_view_.Release();
}

bool HTMLFrameOwnerElement::LoadOrRedirectSubframe(
    const KURL& url,
    const AtomicString& frame_name,
    bool replace_current_item) {
  UpdateContainerPolicy();

  if (ContentFrame()) {
    ContentFrame()->Navigate(GetDocument(), url, replace_current_item,
                             UserGestureStatus::kNone);
    return true;
  }

  if (!SubframeLoadingDisabler::CanLoadFrame(*this))
    return false;

  if (GetDocument().GetFrame()->GetPage()->SubframeCount() >=
      Page::kMaxNumberOfFrames)
    return false;

  LocalFrame* child_frame =
      GetDocument().GetFrame()->Client()->CreateFrame(frame_name, this);
  DCHECK_EQ(ContentFrame(), child_frame);
  if (!child_frame)
    return false;

  ResourceRequest request(url);
  ReferrerPolicy policy = ReferrerPolicyAttribute();
  if (policy != kReferrerPolicyDefault) {
    request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
        policy, url, GetDocument().OutgoingReferrer()));
  }

  FrameLoadType child_load_type = kFrameLoadTypeInitialInChildFrame;
  if (!GetDocument().LoadEventFinished() &&
      GetDocument().Loader()->LoadType() ==
          kFrameLoadTypeReloadBypassingCache) {
    child_load_type = kFrameLoadTypeReloadBypassingCache;
    request.SetCachePolicy(WebCachePolicy::kBypassingCache);
  }

  child_frame->Loader().Load(FrameLoadRequest(&GetDocument(), request),
                             child_load_type);
  return true;
}

DEFINE_TRACE(HTMLFrameOwnerElement) {
  visitor->Trace(content_frame_);
  visitor->Trace(embedded_content_view_);
  HTMLElement::Trace(visitor);
  FrameOwner::Trace(visitor);
}

}  // namespace blink
