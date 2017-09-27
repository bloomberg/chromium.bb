/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2009 Rob Buis (rwlbuis@gmail.com)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 */

#include "core/html/HTMLLinkElement.h"

#include "bindings/core/v8/ScriptEventListener.h"
#include "core/CoreInitializer.h"
#include "core/HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/events/Event.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/UseCounter.h"
#include "core/html/CrossOriginAttribute.h"
#include "core/html/LinkManifest.h"
#include "core/html/imports/LinkImport.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/NetworkHintsInterface.h"
#include "core/origin_trials/OriginTrials.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/WebIconSizesParser.h"
#include "public/platform/WebSize.h"

namespace blink {

using namespace HTMLNames;

inline HTMLLinkElement::HTMLLinkElement(Document& document,
                                        bool created_by_parser)
    : HTMLElement(linkTag, document),
      link_loader_(LinkLoader::Create(this)),
      referrer_policy_(kReferrerPolicyDefault),
      sizes_(DOMTokenList::Create(*this, HTMLNames::sizesAttr)),
      rel_list_(RelList::Create(this)),
      created_by_parser_(created_by_parser) {}

HTMLLinkElement* HTMLLinkElement::Create(Document& document,
                                         bool created_by_parser) {
  return new HTMLLinkElement(document, created_by_parser);
}

HTMLLinkElement::~HTMLLinkElement() {}

void HTMLLinkElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == relAttr) {
    rel_attribute_ = LinkRelAttribute(value);
    rel_list_->DidUpdateAttributeValue(params.old_value, value);
    Process();
  } else if (name == hrefAttr) {
    // Log href attribute before logging resource fetching in process().
    LogUpdateAttributeIfIsolatedWorldAndInDocument("link", params);
    Process();
  } else if (name == typeAttr) {
    type_ = value;
    Process();
  } else if (name == asAttr) {
    as_ = value;
    Process();
  } else if (name == referrerpolicyAttr) {
    if (!value.IsNull()) {
      SecurityPolicy::ReferrerPolicyFromString(
          value, kDoNotSupportReferrerPolicyLegacyKeywords, &referrer_policy_);
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLLinkElementReferrerPolicyAttribute);
    }
  } else if (name == sizesAttr) {
    sizes_->DidUpdateAttributeValue(params.old_value, value);
    WebVector<WebSize> web_icon_sizes =
        WebIconSizesParser::ParseIconSizes(value);
    icon_sizes_.resize(web_icon_sizes.size());
    for (size_t i = 0; i < web_icon_sizes.size(); ++i)
      icon_sizes_[i] = web_icon_sizes[i];
    Process();
  } else if (name == mediaAttr) {
    media_ = value.DeprecatedLower();
    Process();
  } else if (name == scopeAttr) {
    scope_ = value;
    Process();
  } else if (name == disabledAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kHTMLLinkElementDisabled);
    if (LinkStyle* link = GetLinkStyle())
      link->SetDisabledState(!value.IsNull());
  } else {
    if (name == titleAttr) {
      if (LinkStyle* link = GetLinkStyle())
        link->SetSheetTitle(value);
    }

    HTMLElement::ParseAttribute(params);
  }
}

bool HTMLLinkElement::ShouldLoadLink() {
  const KURL& href = GetNonEmptyURLAttribute(hrefAttr);
  return (IsInDocumentTree() ||
          (isConnected() && rel_attribute_.IsStyleSheet())) &&
         !href.PotentiallyDanglingMarkup();
}

bool HTMLLinkElement::LoadLink(const String& type,
                               const String& as,
                               const String& media,
                               const String& nonce,
                               ReferrerPolicy referrer_policy,
                               const KURL& url) {
  return link_loader_->LoadLink(rel_attribute_,
                                GetCrossOriginAttributeValue(FastGetAttribute(
                                    HTMLNames::crossoriginAttr)),
                                type, as, media, nonce, referrer_policy, url,
                                GetDocument(), NetworkHintsInterfaceImpl());
}

LinkResource* HTMLLinkElement::LinkResourceToProcess() {
  if (!ShouldLoadLink()) {
    DCHECK(!GetLinkStyle() || !GetLinkStyle()->HasSheet());
    // TODO(yoav): Ideally, the element's error event would be fired here.
    return nullptr;
  }

  if (!link_) {
    if (rel_attribute_.IsImport()) {
      link_ = LinkImport::Create(this);
    } else if (rel_attribute_.IsManifest()) {
      link_ = LinkManifest::Create(this);
    } else if (rel_attribute_.IsServiceWorker() &&
               OriginTrials::linkServiceWorkerEnabled(GetExecutionContext())) {
      if (GetDocument().GetFrame()) {
        link_ = CoreInitializer::GetInstance().CreateServiceWorkerLinkResource(
            this);
      }
    } else {
      LinkStyle* link = LinkStyle::Create(this);
      if (FastHasAttribute(disabledAttr)) {
        UseCounter::Count(GetDocument(), WebFeature::kHTMLLinkElementDisabled);
        link->SetDisabledState(true);
      }
      link_ = link;
    }
  }

  return link_.Get();
}

LinkStyle* HTMLLinkElement::GetLinkStyle() const {
  if (!link_ || link_->GetType() != LinkResource::kStyle)
    return nullptr;
  return static_cast<LinkStyle*>(link_.Get());
}

LinkImport* HTMLLinkElement::GetLinkImport() const {
  if (!link_ || link_->GetType() != LinkResource::kImport)
    return nullptr;
  return static_cast<LinkImport*>(link_.Get());
}

Document* HTMLLinkElement::import() const {
  if (LinkImport* link = GetLinkImport())
    return link->ImportedDocument();
  return nullptr;
}

void HTMLLinkElement::Process() {
  if (LinkResource* link = LinkResourceToProcess())
    link->Process();
}

Node::InsertionNotificationRequest HTMLLinkElement::InsertedInto(
    ContainerNode* insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  LogAddElementIfIsolatedWorldAndInDocument("link", relAttr, hrefAttr);
  if (!insertion_point->isConnected())
    return kInsertionDone;
  DCHECK(isConnected());
  if (!ShouldLoadLink() && IsInShadowTree()) {
    String message = "HTML element <link> is ignored in shadow tree.";
    GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kWarningMessageLevel, message));
    return kInsertionDone;
  }

  GetDocument().GetStyleEngine().AddStyleSheetCandidateNode(*this);

  Process();

  if (link_)
    link_->OwnerInserted();

  return kInsertionDone;
}

void HTMLLinkElement::RemovedFrom(ContainerNode* insertion_point) {
  // Store the result of isConnected() here before Node::removedFrom(..) clears
  // the flags.
  bool was_connected = isConnected();
  HTMLElement::RemovedFrom(insertion_point);
  if (!insertion_point->isConnected())
    return;

  link_loader_->Abort();

  if (!was_connected) {
    DCHECK(!GetLinkStyle() || !GetLinkStyle()->HasSheet());
    return;
  }
  GetDocument().GetStyleEngine().RemoveStyleSheetCandidateNode(
      *this, *insertion_point);
  if (link_)
    link_->OwnerRemoved();
}

void HTMLLinkElement::FinishParsingChildren() {
  created_by_parser_ = false;
  HTMLElement::FinishParsingChildren();
}

bool HTMLLinkElement::HasActivationBehavior() const {
  // TODO(tkent): Implement activation behavior. crbug.com/422732.
  return false;
}

bool HTMLLinkElement::StyleSheetIsLoading() const {
  return GetLinkStyle() && GetLinkStyle()->StyleSheetIsLoading();
}

void HTMLLinkElement::LinkLoaded() {
  DispatchEvent(Event::Create(EventTypeNames::load));
}

void HTMLLinkElement::LinkLoadingErrored() {
  DispatchEvent(Event::Create(EventTypeNames::error));
}

void HTMLLinkElement::DidStartLinkPrerender() {
  DispatchEvent(Event::Create(EventTypeNames::webkitprerenderstart));
}

void HTMLLinkElement::DidStopLinkPrerender() {
  DispatchEvent(Event::Create(EventTypeNames::webkitprerenderstop));
}

void HTMLLinkElement::DidSendLoadForLinkPrerender() {
  DispatchEvent(Event::Create(EventTypeNames::webkitprerenderload));
}

void HTMLLinkElement::DidSendDOMContentLoadedForLinkPrerender() {
  DispatchEvent(Event::Create(EventTypeNames::webkitprerenderdomcontentloaded));
}

RefPtr<WebTaskRunner> HTMLLinkElement::GetLoadingTaskRunner() {
  return TaskRunnerHelper::Get(TaskType::kNetworking, &GetDocument());
}

bool HTMLLinkElement::SheetLoaded() {
  DCHECK(GetLinkStyle());
  return GetLinkStyle()->SheetLoaded();
}

void HTMLLinkElement::NotifyLoadedSheetAndAllCriticalSubresources(
    LoadedSheetErrorStatus error_status) {
  DCHECK(GetLinkStyle());
  GetLinkStyle()->NotifyLoadedSheetAndAllCriticalSubresources(error_status);
}

void HTMLLinkElement::DispatchPendingEvent(
    std::unique_ptr<IncrementLoadEventDelayCount> count) {
  DCHECK(link_);
  if (link_->HasLoaded())
    LinkLoaded();
  else
    LinkLoadingErrored();

  // Checks Document's load event synchronously here for performance.
  // This is safe because dispatchPendingEvent() is called asynchronously.
  count->ClearAndCheckLoadEvent();
}

void HTMLLinkElement::ScheduleEvent() {
  TaskRunnerHelper::Get(TaskType::kDOMManipulation, &GetDocument())
      ->PostTask(BLINK_FROM_HERE,
                 WTF::Bind(&HTMLLinkElement::DispatchPendingEvent,
                           WrapPersistent(this),
                           WTF::Passed(IncrementLoadEventDelayCount::Create(
                               GetDocument()))));
}

void HTMLLinkElement::StartLoadingDynamicSheet() {
  DCHECK(GetLinkStyle());
  GetLinkStyle()->StartLoadingDynamicSheet();
}

bool HTMLLinkElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName().LocalName() == hrefAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLLinkElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == hrefAttr || HTMLElement::HasLegalLinkAttribute(name);
}

const QualifiedName& HTMLLinkElement::SubResourceAttributeName() const {
  // If the link element is not css, ignore it.
  if (DeprecatedEqualIgnoringCase(getAttribute(typeAttr), "text/css")) {
    // FIXME: Add support for extracting links of sub-resources which
    // are inside style-sheet such as @import, @font-face, url(), etc.
    return hrefAttr;
  }
  return HTMLElement::SubResourceAttributeName();
}

KURL HTMLLinkElement::Href() const {
  const String& url = getAttribute(hrefAttr);
  if (url.IsEmpty())
    return KURL();
  return GetDocument().CompleteURL(url);
}

const AtomicString& HTMLLinkElement::Rel() const {
  return getAttribute(relAttr);
}

const AtomicString& HTMLLinkElement::GetType() const {
  return getAttribute(typeAttr);
}

bool HTMLLinkElement::Async() const {
  return FastHasAttribute(HTMLNames::asyncAttr);
}

IconType HTMLLinkElement::GetIconType() const {
  return rel_attribute_.GetIconType();
}

const Vector<IntSize>& HTMLLinkElement::IconSizes() const {
  return icon_sizes_;
}

DOMTokenList* HTMLLinkElement::sizes() const {
  return sizes_.Get();
}

DEFINE_TRACE(HTMLLinkElement) {
  visitor->Trace(link_);
  visitor->Trace(sizes_);
  visitor->Trace(link_loader_);
  visitor->Trace(rel_list_);
  HTMLElement::Trace(visitor);
  LinkLoaderClient::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(HTMLLinkElement) {
  visitor->TraceWrappers(rel_list_);
  HTMLElement::TraceWrappers(visitor);
}

}  // namespace blink
