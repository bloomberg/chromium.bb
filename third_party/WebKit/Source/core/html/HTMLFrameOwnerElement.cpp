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
#include "core/dom/events/Event.h"
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
#include "public/platform/modules/fetch/fetch_api_request.mojom-shared.h"

namespace blink {

namespace {

using PluginSet = PersistentHeapHashSet<Member<PluginView>>;
PluginSet& PluginsPendingDispose() {
  DEFINE_STATIC_LOCAL(PluginSet, set, ());
  return set;
}

}  // namespace

SubframeLoadingDisabler::SubtreeRootSet&
SubframeLoadingDisabler::DisabledSubtreeRoots() {
  DEFINE_STATIC_LOCAL(SubtreeRootSet, nodes, ());
  return nodes;
}

// static
int HTMLFrameOwnerElement::PluginDisposeSuspendScope::suspend_count_ = 0;

void HTMLFrameOwnerElement::PluginDisposeSuspendScope::
    PerformDeferredPluginDispose() {
  DCHECK_EQ(suspend_count_, 1);
  suspend_count_ = 0;

  PluginSet dispose_set;
  PluginsPendingDispose().swap(dispose_set);
  for (const auto& plugin : dispose_set) {
    plugin->Dispose();
  }
}

HTMLFrameOwnerElement::HTMLFrameOwnerElement(const QualifiedName& tag_name,
                                             Document& document)
    : HTMLElement(tag_name, document),
      content_frame_(nullptr),
      embedded_content_view_(nullptr),
      sandbox_flags_(kSandboxNone),
      did_load_non_empty_document_(false) {}

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
  if (!ContentFrame())
    return;

  // Removing a subframe that was still loading can impact the result of
  // AllDescendantsAreComplete that is consulted by Document::ShouldComplete.
  // Therefore we might need to re-check this after removing the subframe.  The
  // re-check is not needed for local frames (which will handle re-checking from
  // FrameLoader::DidFinishNavigation that responds to LocalFrame::Detach).
  // OTOH, re-checking is required for OOPIFs - see https://crbug.com/779433.
  Document& parent_doc = GetDocument();
  bool have_to_check_if_parent_is_completed = !parent_doc.IsLoadCompleted() &&
                                              ContentFrame()->IsRemoteFrame() &&
                                              ContentFrame()->IsLoading();

  // FIXME: Currently we don't do this in removedFrom because this causes an
  // unload event in the subframe which could execute script that could then
  // reach up into this document and then attempt to look back down. We should
  // see if this behavior is really needed as Gecko does not allow this.
  ContentFrame()->Detach(FrameDetachType::kRemove);

  // Check if removing the subframe caused |parent_doc| to finish loading.
  if (have_to_check_if_parent_is_completed)
    parent_doc.CheckCompleted();
}

HTMLFrameOwnerElement::~HTMLFrameOwnerElement() {
  // An owner must by now have been informed of detachment
  // when the frame was closed.
  DCHECK(!content_frame_);
}

Document* HTMLFrameOwnerElement::contentDocument() const {
  return (content_frame_ && content_frame_->IsLocalFrame())
             ? ToLocalFrame(content_frame_)->GetDocument()
             : nullptr;
}

DOMWindow* HTMLFrameOwnerElement::contentWindow() const {
  return content_frame_ ? content_frame_->DomWindow() : nullptr;
}

void HTMLFrameOwnerElement::SetSandboxFlags(SandboxFlags flags) {
  sandbox_flags_ = flags;
  // Recalculate the container policy in case the allow-same-origin flag has
  // changed.
  container_policy_ = ConstructContainerPolicy(nullptr, nullptr);

  // Don't notify about updates if ContentFrame() is null, for example when
  // the subframe hasn't been created yet.
  if (ContentFrame()) {
    GetDocument().GetFrame()->Client()->DidChangeFramePolicy(
        ContentFrame(), sandbox_flags_, container_policy_);
  }
}

bool HTMLFrameOwnerElement::IsKeyboardFocusable() const {
  return content_frame_ && HTMLElement::IsKeyboardFocusable();
}

void HTMLFrameOwnerElement::DisposePluginSoon(PluginView* plugin) {
  if (PluginDisposeSuspendScope::suspend_count_) {
    PluginsPendingDispose().insert(plugin);
    PluginDisposeSuspendScope::suspend_count_ |= 1;
  } else
    plugin->Dispose();
}

void HTMLFrameOwnerElement::UpdateContainerPolicy(Vector<String>* messages,
                                                  bool* old_syntax) {
  container_policy_ = ConstructContainerPolicy(messages, old_syntax);
  // Don't notify about updates if ContentFrame() is null, for example when
  // the subframe hasn't been created yet.
  if (ContentFrame()) {
    GetDocument().GetFrame()->Client()->DidChangeFramePolicy(
        ContentFrame(), sandbox_flags_, container_policy_);
  }
}

void HTMLFrameOwnerElement::FrameOwnerPropertiesChanged() {
  // Don't notify about updates if ContentFrame() is null, for example when
  // the subframe hasn't been created yet.
  if (ContentFrame()) {
    GetDocument().GetFrame()->Client()->DidChangeFrameOwnerProperties(this);
  }
}

void HTMLFrameOwnerElement::DispatchLoad() {
  DispatchScopedEvent(Event::Create(EventTypeNames::load));
}

const ParsedFeaturePolicy& HTMLFrameOwnerElement::ContainerPolicy() const {
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
    // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout
    // crashes.  Perhaps view is getting reattached while document is shutting
    // down.
    if (doc) {
      CHECK_NE(doc->Lifecycle().GetState(), DocumentLifecycle::kStopping);
    }
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
    request.SetCacheMode(mojom::FetchCacheMode::kBypassCache);
  }

  child_frame->Loader().Load(FrameLoadRequest(&GetDocument(), request),
                             child_load_type);
  return true;
}

void HTMLFrameOwnerElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(content_frame_);
  visitor->Trace(embedded_content_view_);
  HTMLElement::Trace(visitor);
  FrameOwner::Trace(visitor);
}

}  // namespace blink
