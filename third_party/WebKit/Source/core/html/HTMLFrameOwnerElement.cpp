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
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/plugins/PluginView.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

typedef HeapHashMap<Member<FrameViewBase>, Member<FrameView>>
    FrameViewBaseToParentMap;
static FrameViewBaseToParentMap& widgetNewParentMap() {
  DEFINE_STATIC_LOCAL(FrameViewBaseToParentMap, map,
                      (new FrameViewBaseToParentMap));
  return map;
}

using FrameViewBaseSet = HeapHashSet<Member<FrameViewBase>>;
static FrameViewBaseSet& widgetsPendingTemporaryRemovalFromParent() {
  // FrameViewBases in this set will not leak because it will be cleared in
  // HTMLFrameOwnerElement::UpdateSuspendScope::performDeferredWidgetTreeOperations.
  DEFINE_STATIC_LOCAL(FrameViewBaseSet, set, (new FrameViewBaseSet));
  return set;
}

static FrameViewBaseSet& widgetsPendingDispose() {
  DEFINE_STATIC_LOCAL(FrameViewBaseSet, set, (new FrameViewBaseSet));
  return set;
}

SubframeLoadingDisabler::SubtreeRootSet&
SubframeLoadingDisabler::disabledSubtreeRoots() {
  DEFINE_STATIC_LOCAL(SubtreeRootSet, nodes, ());
  return nodes;
}

static unsigned s_updateSuspendCount = 0;

HTMLFrameOwnerElement::UpdateSuspendScope::UpdateSuspendScope() {
  ++s_updateSuspendCount;
}

void HTMLFrameOwnerElement::UpdateSuspendScope::
    performDeferredWidgetTreeOperations() {
  FrameViewBaseToParentMap map;
  widgetNewParentMap().swap(map);
  for (const auto& frameViewBase : map) {
    FrameViewBase* child = frameViewBase.key.get();
    FrameView* currentParent = toFrameView(child->parent());
    FrameView* newParent = frameViewBase.value;
    if (newParent != currentParent) {
      if (currentParent)
        currentParent->removeChild(child);
      if (newParent)
        newParent->addChild(child);
      if (currentParent && !newParent)
        child->dispose();
    }
  }

  {
    FrameViewBaseSet set;
    widgetsPendingTemporaryRemovalFromParent().swap(set);
    for (const auto& frameViewBase : set) {
      FrameView* currentParent = toFrameView(frameViewBase->parent());
      if (currentParent)
        currentParent->removeChild(frameViewBase.get());
    }
  }

  {
    FrameViewBaseSet set;
    widgetsPendingDispose().swap(set);
    for (const auto& frameViewBase : set) {
      frameViewBase->dispose();
    }
  }
}

HTMLFrameOwnerElement::UpdateSuspendScope::~UpdateSuspendScope() {
  DCHECK_GT(s_updateSuspendCount, 0u);
  if (s_updateSuspendCount == 1)
    performDeferredWidgetTreeOperations();
  --s_updateSuspendCount;
}

// Unlike moveWidgetToParentSoon, this will not call dispose the Widget.
void temporarilyRemoveWidgetFromParentSoon(FrameViewBase* frameViewBase) {
  if (s_updateSuspendCount) {
    widgetsPendingTemporaryRemovalFromParent().insert(frameViewBase);
  } else {
    if (toFrameView(frameViewBase->parent()))
      toFrameView(frameViewBase->parent())->removeChild(frameViewBase);
  }
}

void moveWidgetToParentSoon(FrameViewBase* child, FrameView* parent) {
  if (!s_updateSuspendCount) {
    if (parent) {
      parent->addChild(child);
    } else if (toFrameView(child->parent())) {
      toFrameView(child->parent())->removeChild(child);
      child->dispose();
    }
    return;
  }
  widgetNewParentMap().set(child, parent);
}

HTMLFrameOwnerElement::HTMLFrameOwnerElement(const QualifiedName& tagName,
                                             Document& document)
    : HTMLElement(tagName, document),
      m_contentFrame(nullptr),
      m_widget(nullptr),
      m_sandboxFlags(SandboxNone) {}

LayoutPart* HTMLFrameOwnerElement::layoutPart() const {
  // HTMLObjectElement and HTMLEmbedElement may return arbitrary layoutObjects
  // when using fallback content.
  if (!layoutObject() || !layoutObject()->isLayoutPart())
    return nullptr;
  return toLayoutPart(layoutObject());
}

void HTMLFrameOwnerElement::setContentFrame(Frame& frame) {
  // Make sure we will not end up with two frames referencing the same owner
  // element.
  DCHECK(!m_contentFrame || m_contentFrame->owner() != this);
  // Disconnected frames should not be allowed to load.
  DCHECK(isConnected());
  m_contentFrame = &frame;

  for (ContainerNode* node = this; node; node = node->parentOrShadowHostNode())
    node->incrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::clearContentFrame() {
  if (!m_contentFrame)
    return;

  DCHECK_EQ(m_contentFrame->owner(), this);
  m_contentFrame = nullptr;

  for (ContainerNode* node = this; node; node = node->parentOrShadowHostNode())
    node->decrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::disconnectContentFrame() {
  // FIXME: Currently we don't do this in removedFrom because this causes an
  // unload event in the subframe which could execute script that could then
  // reach up into this document and then attempt to look back down. We should
  // see if this behavior is really needed as Gecko does not allow this.
  if (Frame* frame = contentFrame()) {
    frame->detach(FrameDetachType::Remove);
  }
}

HTMLFrameOwnerElement::~HTMLFrameOwnerElement() {
  // An owner must by now have been informed of detachment
  // when the frame was closed.
  DCHECK(!m_contentFrame);
}

Document* HTMLFrameOwnerElement::contentDocument() const {
  return (m_contentFrame && m_contentFrame->isLocalFrame())
             ? toLocalFrame(m_contentFrame)->document()
             : 0;
}

DOMWindow* HTMLFrameOwnerElement::contentWindow() const {
  return m_contentFrame ? m_contentFrame->domWindow() : 0;
}

void HTMLFrameOwnerElement::setSandboxFlags(SandboxFlags flags) {
  m_sandboxFlags = flags;
  // Don't notify about updates if contentFrame() is null, for example when
  // the subframe hasn't been created yet.
  if (contentFrame())
    document().frame()->loader().client()->didChangeSandboxFlags(contentFrame(),
                                                                 flags);
}

bool HTMLFrameOwnerElement::isKeyboardFocusable() const {
  return m_contentFrame && HTMLElement::isKeyboardFocusable();
}

void HTMLFrameOwnerElement::disposeWidgetSoon(FrameViewBase* frameViewBase) {
  if (s_updateSuspendCount) {
    widgetsPendingDispose().insert(frameViewBase);
    return;
  }
  frameViewBase->dispose();
}

void HTMLFrameOwnerElement::dispatchLoad() {
  dispatchScopedEvent(Event::create(EventTypeNames::load));
}

const WebVector<mojom::blink::PermissionName>&
HTMLFrameOwnerElement::delegatedPermissions() const {
  DEFINE_STATIC_LOCAL(WebVector<mojom::blink::PermissionName>, permissions, ());
  return permissions;
}

const WebVector<WebFeaturePolicyFeature>&
HTMLFrameOwnerElement::allowedFeatures() const {
  DEFINE_STATIC_LOCAL(WebVector<WebFeaturePolicyFeature>, features, ());
  return features;
}

Document* HTMLFrameOwnerElement::getSVGDocument(
    ExceptionState& exceptionState) const {
  Document* doc = contentDocument();
  if (doc && doc->isSVGDocument())
    return doc;
  return nullptr;
}

void HTMLFrameOwnerElement::setWidget(FrameViewBase* frameViewBase) {
  if (frameViewBase == m_widget)
    return;

  if (m_widget) {
    if (m_widget->parent())
      moveWidgetToParentSoon(m_widget.get(), 0);
    m_widget = nullptr;
  }

  m_widget = frameViewBase;

  LayoutPart* layoutPart = toLayoutPart(layoutObject());
  LayoutPartItem layoutPartItem = LayoutPartItem(layoutPart);
  if (layoutPartItem.isNull())
    return;

  if (m_widget) {
    layoutPartItem.updateOnWidgetChange();

    DCHECK_EQ(document().view(), layoutPartItem.frameView());
    DCHECK(layoutPartItem.frameView());
    moveWidgetToParentSoon(m_widget.get(), layoutPartItem.frameView());
  }

  if (AXObjectCache* cache = document().existingAXObjectCache())
    cache->childrenChanged(layoutPart);
}

FrameViewBase* HTMLFrameOwnerElement::releaseWidget() {
  if (!m_widget)
    return nullptr;
  if (m_widget->parent())
    temporarilyRemoveWidgetFromParentSoon(m_widget.get());
  LayoutPart* layoutPart = toLayoutPart(layoutObject());
  if (layoutPart) {
    if (AXObjectCache* cache = document().existingAXObjectCache())
      cache->childrenChanged(layoutPart);
  }
  return m_widget.release();
}

FrameViewBase* HTMLFrameOwnerElement::ownedWidget() const {
  return m_widget.get();
}

bool HTMLFrameOwnerElement::loadOrRedirectSubframe(
    const KURL& url,
    const AtomicString& frameName,
    bool replaceCurrentItem) {
  LocalFrame* parentFrame = document().frame();
  if (contentFrame()) {
    contentFrame()->navigate(document(), url, replaceCurrentItem,
                             UserGestureStatus::None);
    return true;
  }

  if (!document().getSecurityOrigin()->canDisplay(url)) {
    FrameLoader::reportLocalLoadFailed(parentFrame, url.getString());
    return false;
  }

  if (!SubframeLoadingDisabler::canLoadFrame(*this))
    return false;

  if (document().frame()->host()->subframeCount() >=
      FrameHost::maxNumberOfFrames)
    return false;

  FrameLoadRequest frameLoadRequest(&document(), url, "_self",
                                    CheckContentSecurityPolicy);

  ReferrerPolicy policy = referrerPolicyAttribute();
  if (policy != ReferrerPolicyDefault)
    frameLoadRequest.resourceRequest().setHTTPReferrer(
        SecurityPolicy::generateReferrer(policy, url,
                                         document().outgoingReferrer()));

  return parentFrame->loader().client()->createFrame(frameLoadRequest,
                                                     frameName, this);
}

DEFINE_TRACE(HTMLFrameOwnerElement) {
  visitor->trace(m_contentFrame);
  visitor->trace(m_widget);
  HTMLElement::trace(visitor);
  FrameOwner::trace(visitor);
}

}  // namespace blink
