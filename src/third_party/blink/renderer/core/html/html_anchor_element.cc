/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#include "third_party/blink/renderer/core/html/html_anchor_element.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/impression_conversions.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"
#include "third_party/blink/renderer/core/html/conversion_measurement_parsing.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "third_party/blink/renderer/core/loader/ping_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

namespace {

// The download attribute specifies a filename, and an excessively long one can
// crash the browser process. Filepaths probably can't be longer than 4096
// characters, but this is enough to prevent the browser process from becoming
// unresponsive or crashing.
const int kMaxDownloadAttrLength = 1000000;

// Note: Here it covers download originated from clicking on <a download> link
// that results in direct download. Features in this method can also be logged
// from browser for download due to navigations to non-web-renderable content.
bool ShouldInterveneDownloadByFramePolicy(LocalFrame* frame) {
  bool should_intervene_download = false;
  Document& document = *(frame->GetDocument());
  UseCounter::Count(document, WebFeature::kDownloadPrePolicyCheck);
  bool has_gesture = LocalFrame::HasTransientUserActivation(frame);
  if (!has_gesture) {
    UseCounter::Count(document, WebFeature::kDownloadWithoutUserGesture);
  }
  if (frame->IsAdSubframe()) {
    UseCounter::Count(document, WebFeature::kDownloadInAdFrame);
    if (!has_gesture) {
      UseCounter::Count(document,
                        WebFeature::kDownloadInAdFrameWithoutUserGesture);
      if (base::FeatureList::IsEnabled(
              blink::features::
                  kBlockingDownloadsInAdFrameWithoutUserActivation))
        should_intervene_download = true;
    }
  }
  if (frame->DomWindow()->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kDownloads)) {
    UseCounter::Count(document, WebFeature::kDownloadInSandbox);
    should_intervene_download = true;
  }
  if (!should_intervene_download)
    UseCounter::Count(document, WebFeature::kDownloadPostPolicyCheck);
  return should_intervene_download;
}

}  // namespace

HTMLAnchorElement::HTMLAnchorElement(Document& document)
    : HTMLAnchorElement(html_names::kATag, document) {}

HTMLAnchorElement::HTMLAnchorElement(const QualifiedName& tag_name,
                                     Document& document)
    : HTMLElement(tag_name, document),
      link_relations_(0),
      cached_visited_link_hash_(0),
      rel_list_(MakeGarbageCollected<RelList>(this)) {}

HTMLAnchorElement::~HTMLAnchorElement() = default;

bool HTMLAnchorElement::SupportsFocus() const {
  if (HasEditableStyle(*this))
    return HTMLElement::SupportsFocus();
  // If not a link we should still be able to focus the element if it has
  // tabIndex.
  return IsLink() || HTMLElement::SupportsFocus();
}

bool HTMLAnchorElement::ShouldHaveFocusAppearance() const {
  return (GetDocument().LastFocusType() != mojom::blink::FocusType::kMouse) ||
         HTMLElement::SupportsFocus();
}

bool HTMLAnchorElement::IsMouseFocusable() const {
  if (!IsFocusableStyleAfterUpdate())
    return false;
  if (IsLink())
    return SupportsFocus();

  return HTMLElement::IsMouseFocusable();
}

bool HTMLAnchorElement::IsKeyboardFocusable() const {
  if (!IsFocusableStyleAfterUpdate())
    return false;

  // Anchor is focusable if the base element supports focus and is focusable.
  if (IsBaseElementFocusable() && Element::SupportsFocus())
    return HTMLElement::IsKeyboardFocusable();

  if (IsLink() && !GetDocument().GetPage()->GetChromeClient().TabsToLinks())
    return false;
  return HTMLElement::IsKeyboardFocusable();
}

static void AppendServerMapMousePosition(StringBuilder& url, Event* event) {
  auto* mouse_event = DynamicTo<MouseEvent>(event);
  if (!mouse_event)
    return;

  DCHECK(event->target());
  Node* target = event->target()->ToNode();
  DCHECK(target);
  auto* image_element = DynamicTo<HTMLImageElement>(target);
  if (!image_element || !image_element->IsServerMap())
    return;

  LayoutObject* layout_object = image_element->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return;

  // The coordinates sent in the query string are relative to the height and
  // width of the image element, ignoring CSS transform/zoom.
  FloatPoint map_point = layout_object->AbsoluteToLocalFloatPoint(
      FloatPoint(mouse_event->AbsoluteLocation()));

  // The origin (0,0) is at the upper left of the content area, inside the
  // padding and border.
  map_point -=
      FloatSize(To<LayoutBox>(layout_object)->PhysicalContentBoxOffset());

  // CSS zoom is not reflected in the map coordinates.
  float scale_factor = 1 / layout_object->Style()->EffectiveZoom();
  map_point.Scale(scale_factor, scale_factor);

  // Negative coordinates are clamped to 0 such that clicks in the left and
  // top padding/border areas receive an X or Y coordinate of 0.
  IntPoint clamped_point(RoundedIntPoint(map_point));
  clamped_point.ClampNegativeToZero();

  url.Append('?');
  url.AppendNumber(clamped_point.X());
  url.Append(',');
  url.AppendNumber(clamped_point.Y());
}

void HTMLAnchorElement::DefaultEventHandler(Event& event) {
  if (IsLink()) {
    if (IsFocused() && IsEnterKeyKeydownEvent(event) && IsLiveLink()) {
      event.SetDefaultHandled();
      DispatchSimulatedClick(&event);
      return;
    }

    if (IsLinkClick(event) && IsLiveLink()) {
      HandleClick(event);
      return;
    }
  }

  HTMLElement::DefaultEventHandler(event);
}

bool HTMLAnchorElement::HasActivationBehavior() const {
  return true;
}

void HTMLAnchorElement::SetActive(bool active) {
  if (active && HasEditableStyle(*this))
    return;

  HTMLElement::SetActive(active);
}

void HTMLAnchorElement::AttributeChanged(
    const AttributeModificationParams& params) {
  HTMLElement::AttributeChanged(params);
  if (params.name == html_names::kRegisterattributionsourceAttr &&
      !params.new_value.IsNull() && HasImpression()) {
    absl::optional<WebImpression> impression = GetImpressionForAnchor(this);
    if (impression) {
      mojo::AssociatedRemote<mojom::blink::ConversionHost> conversion_host;
      GetDocument()
          .GetFrame()
          ->GetRemoteNavigationAssociatedInterfaces()
          ->GetInterface(&conversion_host);
      conversion_host->RegisterImpression(
          ConvertWebImpressionToImpression(*impression));
    }
  }

  if (params.reason != AttributeModificationReason::kDirectly)
    return;
  if (params.name != html_names::kHrefAttr)
    return;
  if (!IsLink() && AdjustedFocusedElementInTreeScope() == this)
    blur();
}

void HTMLAnchorElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kHrefAttr) {
    bool was_link = IsLink();
    SetIsLink(!params.new_value.IsNull());
    if (was_link || IsLink()) {
      PseudoStateChanged(CSSSelector::kPseudoLink);
      PseudoStateChanged(CSSSelector::kPseudoVisited);
      PseudoStateChanged(CSSSelector::kPseudoWebkitAnyLink);
      PseudoStateChanged(CSSSelector::kPseudoAnyLink);
    }
    if (IsLink()) {
      String parsed_url = StripLeadingAndTrailingHTMLSpaces(params.new_value);
      // GetDocument().GetFrame() could be null if this method is called from
      // DOMParser::parseFromString(), which internally creates a document
      // and eventually calls this.
      if (GetDocument().IsDNSPrefetchEnabled() && GetDocument().GetFrame()) {
        if (ProtocolIs(parsed_url, "http") || ProtocolIs(parsed_url, "https") ||
            parsed_url.StartsWith("//")) {
          WebPrescientNetworking* web_prescient_networking =
              GetDocument().GetFrame()->PrescientNetworking();
          if (web_prescient_networking) {
            web_prescient_networking->PrefetchDNS(
                GetDocument().CompleteURL(parsed_url).Host());
          }
        }
      }
    }
    InvalidateCachedVisitedLinkHash();
    LogUpdateAttributeIfIsolatedWorldAndInDocument("a", params);
  } else if (params.name == html_names::kNameAttr ||
             params.name == html_names::kTitleAttr) {
    // Do nothing.
  } else if (params.name == html_names::kRelAttr) {
    SetRel(params.new_value);
    rel_list_->DidUpdateAttributeValue(params.old_value, params.new_value);
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

bool HTMLAnchorElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName().LocalName() == html_names::kHrefAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLAnchorElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == html_names::kHrefAttr ||
         HTMLElement::HasLegalLinkAttribute(name);
}

bool HTMLAnchorElement::CanStartSelection() const {
  if (!IsLink())
    return HTMLElement::CanStartSelection();
  return HasEditableStyle(*this);
}

bool HTMLAnchorElement::draggable() const {
  // Should be draggable if we have an href attribute.
  const AtomicString& value = FastGetAttribute(html_names::kDraggableAttr);
  if (EqualIgnoringASCIICase(value, "true"))
    return true;
  if (EqualIgnoringASCIICase(value, "false"))
    return false;
  return FastHasAttribute(html_names::kHrefAttr);
}

KURL HTMLAnchorElement::Href() const {
  return GetDocument().CompleteURL(StripLeadingAndTrailingHTMLSpaces(
      FastGetAttribute(html_names::kHrefAttr)));
}

void HTMLAnchorElement::SetHref(const AtomicString& value) {
  setAttribute(html_names::kHrefAttr, value);
}

KURL HTMLAnchorElement::Url() const {
  return Href();
}

void HTMLAnchorElement::SetURL(const KURL& url) {
  SetHref(AtomicString(url.GetString()));
}

String HTMLAnchorElement::Input() const {
  return FastGetAttribute(html_names::kHrefAttr);
}

void HTMLAnchorElement::setHref(const String& value) {
  SetHref(AtomicString(value));
}

bool HTMLAnchorElement::HasRel(uint32_t relation) const {
  return link_relations_ & relation;
}

void HTMLAnchorElement::SetRel(const AtomicString& value) {
  link_relations_ = 0;
  SpaceSplitString new_link_relations(value.LowerASCII());
  // FIXME: Add link relations as they are implemented
  if (new_link_relations.Contains("noreferrer"))
    link_relations_ |= kRelationNoReferrer;
  if (new_link_relations.Contains("noopener"))
    link_relations_ |= kRelationNoOpener;
  if (new_link_relations.Contains("opener"))
    link_relations_ |= kRelationOpener;
}

const AtomicString& HTMLAnchorElement::GetName() const {
  return GetNameAttribute();
}

const AtomicString& HTMLAnchorElement::GetEffectiveTarget() const {
  const AtomicString& target = FastGetAttribute(html_names::kTargetAttr);
  if (!target.IsEmpty())
    return target;
  return GetDocument().BaseTarget();
}

int HTMLAnchorElement::DefaultTabIndex() const {
  return 0;
}

bool HTMLAnchorElement::IsLiveLink() const {
  return IsLink() && !HasEditableStyle(*this);
}

bool HTMLAnchorElement::HasImpression() const {
  return hasAttribute(html_names::kAttributionsourceeventidAttr) &&
         hasAttribute(html_names::kAttributiondestinationAttr);
}

void HTMLAnchorElement::SendPings(const KURL& destination_url) const {
  const AtomicString& ping_value = FastGetAttribute(html_names::kPingAttr);
  if (ping_value.IsNull() || !GetDocument().GetSettings() ||
      !GetDocument().GetSettings()->GetHyperlinkAuditingEnabled()) {
    return;
  }

  // Pings should not be sent if MHTML page is loaded.
  if (GetDocument().Fetcher()->Archive())
    return;

  if ((ping_value.Contains('\n') || ping_value.Contains('\r') ||
       ping_value.Contains('\t')) &&
      ping_value.Contains('<')) {
    Deprecation::CountDeprecation(
        GetExecutionContext(), WebFeature::kCanRequestURLHTTPContainingNewline);
    return;
  }

  UseCounter::Count(GetDocument(), WebFeature::kHTMLAnchorElementPingAttribute);

  SpaceSplitString ping_urls(ping_value);
  for (unsigned i = 0; i < ping_urls.size(); i++) {
    PingLoader::SendLinkAuditPing(GetDocument().GetFrame(),
                                  GetDocument().CompleteURL(ping_urls[i]),
                                  destination_url);
  }
}

void HTMLAnchorElement::HandleClick(Event& event) {
  event.SetDefaultHandled();

  LocalDOMWindow* window = GetDocument().domWindow();
  if (!window)
    return;

  if (!isConnected()) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kAnchorClickDispatchForNonConnectedNode);
  }

  Document& top_document = GetDocument().TopDocument();
  if (AnchorElementMetricsSender::HasAnchorElementMetricsSender(top_document)) {
    AnchorElementMetricsSender::From(top_document)
        ->MaybeReportClickedMetricsOnClick(*this);
  }

  StringBuilder url;
  url.Append(StripLeadingAndTrailingHTMLSpaces(
      FastGetAttribute(html_names::kHrefAttr)));
  AppendServerMapMousePosition(url, &event);
  KURL completed_url = GetDocument().CompleteURL(url.ToString());

  // Schedule the ping before the frame load. Prerender in Chrome may kill the
  // renderer as soon as the navigation is sent out.
  SendPings(completed_url);

  ResourceRequest request(completed_url);

  network::mojom::ReferrerPolicy policy;
  if (FastHasAttribute(html_names::kReferrerpolicyAttr) &&
      SecurityPolicy::ReferrerPolicyFromString(
          FastGetAttribute(html_names::kReferrerpolicyAttr),
          kSupportReferrerPolicyLegacyKeywords, &policy) &&
      !HasRel(kRelationNoReferrer)) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLAnchorElementReferrerPolicyAttribute);
    request.SetReferrerPolicy(policy);
  }

  LocalFrame* frame = window->GetFrame();
  request.SetHasUserGesture(LocalFrame::HasTransientUserActivation(frame));

  // Respect the download attribute only if we can read the content, and the
  // event is not an alt-click or similar.
  if (FastHasAttribute(html_names::kDownloadAttr) &&
      NavigationPolicyFromEvent(&event) != kNavigationPolicyDownload &&
      window->GetSecurityOrigin()->CanReadContent(completed_url)) {
    if (ShouldInterveneDownloadByFramePolicy(frame))
      return;

    String download_attr =
        static_cast<String>(FastGetAttribute(html_names::kDownloadAttr));
    if (download_attr.length() > kMaxDownloadAttrLength) {
      ConsoleMessage* console_message = MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kError,
          String::Format("Download attribute for anchor element is too long. "
                         "Max: %d, given: %d",
                         kMaxDownloadAttrLength, download_attr.length()));
      console_message->SetNodes(GetDocument().GetFrame(),
                                {DOMNodeIds::IdForNode(this)});
      GetDocument().AddConsoleMessage(console_message);
      return;
    }

    request.SetSuggestedFilename(download_attr);
    request.SetRequestContext(mojom::blink::RequestContextType::DOWNLOAD);
    request.SetRequestorOrigin(window->GetSecurityOrigin());
    network::mojom::ReferrerPolicy referrer_policy =
        request.GetReferrerPolicy();
    if (referrer_policy == network::mojom::ReferrerPolicy::kDefault)
      referrer_policy = window->GetReferrerPolicy();
    Referrer referrer = SecurityPolicy::GenerateReferrer(
        referrer_policy, completed_url, window->OutgoingReferrer());
    request.SetReferrerString(referrer.referrer);
    request.SetReferrerPolicy(referrer.referrer_policy);
    frame->DownloadURL(request, network::mojom::blink::RedirectMode::kManual);
    return;
  }

  request.SetRequestContext(mojom::blink::RequestContextType::HYPERLINK);
  const AtomicString& target = GetEffectiveTarget();
  FrameLoadRequest frame_request(window, request);
  frame_request.SetNavigationPolicy(NavigationPolicyFromEvent(&event));
  frame_request.SetClientRedirectReason(ClientNavigationReason::kAnchorClick);
  if (HasRel(kRelationNoReferrer)) {
    frame_request.SetNoReferrer();
    frame_request.SetNoOpener();
  }
  if (HasRel(kRelationNoOpener) ||
      (EqualIgnoringASCIICase(target, "_blank") && !HasRel(kRelationOpener) &&
       RuntimeEnabledFeatures::TargetBlankImpliesNoOpenerEnabled() &&
       frame->GetSettings()
           ->GetTargetBlankImpliesNoOpenerEnabledWillBeRemoved())) {
    frame_request.SetNoOpener();
  }

  frame_request.SetTriggeringEventInfo(
      event.isTrusted()
          ? mojom::blink::TriggeringEventInfo::kFromTrustedEvent
          : mojom::blink::TriggeringEventInfo::kFromUntrustedEvent);
  frame_request.SetInputStartTime(event.PlatformTimeStamp());

  frame->MaybeLogAdClickNavigation();

  if (request.HasUserGesture() && HasImpression()) {
    // An impression must be attached prior to the
    // FindOrCreateFrameForNavigation() call, as that call may result in
    // performing a navigation if the call results in creating a new window with
    // noopener set.
    // At this time we don't know if the navigation will navigate a main frame
    // or subframe. For example, a middle click on the anchor element will
    // set `target_frame` to `frame`, but end up targeting a new window.
    // Attach the impression regardless, the embedder will be able to drop
    // impressions for subframe navigations.
    absl::optional<WebImpression> impression = GetImpressionForAnchor(this);
    if (impression)
      frame_request.SetImpression(*impression);
  }

  Frame* target_frame =
      frame->Tree().FindOrCreateFrameForNavigation(frame_request, target).frame;

  // If hrefTranslate is enabled and set restrict processing it
  // to same frame or navigations with noopener set.
  if (RuntimeEnabledFeatures::HrefTranslateEnabled(GetExecutionContext()) &&
      FastHasAttribute(html_names::kHreftranslateAttr) &&
      (target_frame == frame || frame_request.GetWindowFeatures().noopener)) {
    frame_request.SetHrefTranslate(
        FastGetAttribute(html_names::kHreftranslateAttr));
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLAnchorElementHrefTranslateAttribute);
  }

  if (target_frame)
    target_frame->Navigate(frame_request, WebFrameLoadType::kStandard);
}

bool IsEnterKeyKeydownEvent(Event& event) {
  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  return event.type() == event_type_names::kKeydown && keyboard_event &&
         keyboard_event->key() == "Enter" && !keyboard_event->repeat();
}

bool IsLinkClick(Event& event) {
  auto* mouse_event = DynamicTo<MouseEvent>(event);
  if ((event.type() != event_type_names::kClick &&
       event.type() != event_type_names::kAuxclick) ||
      !mouse_event) {
    return false;
  }
  int16_t button = mouse_event->button();
  return (button == static_cast<int16_t>(WebPointerProperties::Button::kLeft) ||
          button ==
              static_cast<int16_t>(WebPointerProperties::Button::kMiddle));
}

bool HTMLAnchorElement::WillRespondToMouseClickEvents() {
  return IsLink() || HTMLElement::WillRespondToMouseClickEvents();
}

bool HTMLAnchorElement::IsInteractiveContent() const {
  return IsLink();
}

Node::InsertionNotificationRequest HTMLAnchorElement::InsertedInto(
    ContainerNode& insertion_point) {
  InsertionNotificationRequest request =
      HTMLElement::InsertedInto(insertion_point);
  LogAddElementIfIsolatedWorldAndInDocument("a", html_names::kHrefAttr);

  Document& top_document = GetDocument().TopDocument();
  if (AnchorElementMetricsSender::HasAnchorElementMetricsSender(top_document)) {
    AnchorElementMetricsSender::From(top_document)->AddAnchorElement(*this);
  }

  return request;
}

void HTMLAnchorElement::Trace(Visitor* visitor) const {
  visitor->Trace(rel_list_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
