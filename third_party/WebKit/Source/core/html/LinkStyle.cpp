// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/LinkStyle.h"

#include "core/css/StyleSheetContents.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/SubresourceIntegrity.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/CrossOriginAttribute.h"
#include "core/html/HTMLLinkElement.h"
#include "core/loader/resource/CSSStyleSheetResource.h"
#include "platform/Histogram.h"
#include "platform/network/mime/ContentType.h"
#include "platform/network/mime/MIMETypeRegistry.h"

namespace blink {

using namespace HTMLNames;

static bool StyleSheetTypeIsSupported(const String& type) {
  String trimmed_type = ContentType(type).GetType();
  return trimmed_type.IsEmpty() ||
         MIMETypeRegistry::IsSupportedStyleSheetMIMEType(trimmed_type);
}

LinkStyle* LinkStyle::Create(HTMLLinkElement* owner) {
  return new LinkStyle(owner);
}

LinkStyle::LinkStyle(HTMLLinkElement* owner)
    : LinkResource(owner),
      disabled_state_(kUnset),
      pending_sheet_type_(kNone),
      loading_(false),
      fired_load_(false),
      loaded_sheet_(false),
      fetch_following_cors_(false) {}

LinkStyle::~LinkStyle() {}

Document& LinkStyle::GetDocument() {
  return owner_->GetDocument();
}

enum StyleSheetCacheStatus {
  kStyleSheetNewEntry,
  kStyleSheetInDiskCache,
  kStyleSheetInMemoryCache,
  kStyleSheetCacheStatusCount,
};

void LinkStyle::SetCSSStyleSheet(
    const String& href,
    const KURL& base_url,
    ReferrerPolicy referrer_policy,
    const String& charset,
    const CSSStyleSheetResource* cached_style_sheet) {
  if (!owner_->isConnected()) {
    // While the stylesheet is asynchronously loading, the owner can be
    // disconnected from a document.
    // In that case, cancel any processing on the loaded content.
    loading_ = false;
    RemovePendingSheet();
    if (sheet_)
      ClearSheet();
    return;
  }

  // See the comment in PendingScript.cpp about why this check is necessary
  // here, instead of in the resource fetcher. https://crbug.com/500701.
  if (!cached_style_sheet->ErrorOccurred() &&
      !owner_->FastGetAttribute(HTMLNames::integrityAttr).IsEmpty() &&
      !cached_style_sheet->IntegrityMetadata().IsEmpty()) {
    ResourceIntegrityDisposition disposition =
        cached_style_sheet->IntegrityDisposition();

    if (disposition == ResourceIntegrityDisposition::kNotChecked &&
        !cached_style_sheet->LoadFailedOrCanceled()) {
      bool check_result;

      // cachedStyleSheet->resourceBuffer() can be nullptr on load success.
      // If response size == 0.
      const char* data = nullptr;
      size_t size = 0;
      if (cached_style_sheet->ResourceBuffer()) {
        data = cached_style_sheet->ResourceBuffer()->Data();
        size = cached_style_sheet->ResourceBuffer()->size();
      }
      check_result = SubresourceIntegrity::CheckSubresourceIntegrity(
          owner_->FastGetAttribute(HTMLNames::integrityAttr),
          owner_->GetDocument(), data, size, KURL(base_url, href),
          *cached_style_sheet);
      disposition = check_result ? ResourceIntegrityDisposition::kPassed
                                 : ResourceIntegrityDisposition::kFailed;

      // TODO(kouhei): Remove this const_cast crbug.com/653502
      const_cast<CSSStyleSheetResource*>(cached_style_sheet)
          ->SetIntegrityDisposition(disposition);
    }

    if (disposition == ResourceIntegrityDisposition::kFailed) {
      loading_ = false;
      RemovePendingSheet();
      NotifyLoadedSheetAndAllCriticalSubresources(
          Node::kErrorOccurredLoadingSubresource);
      return;
    }
  }

  CSSParserContext* parser_context = CSSParserContext::Create(
      owner_->GetDocument(), base_url, referrer_policy, charset);

  if (StyleSheetContents* restored_sheet =
          const_cast<CSSStyleSheetResource*>(cached_style_sheet)
              ->RestoreParsedStyleSheet(parser_context)) {
    DCHECK(restored_sheet->IsCacheableForResource());
    DCHECK(!restored_sheet->IsLoading());

    if (sheet_)
      ClearSheet();
    sheet_ = CSSStyleSheet::Create(restored_sheet, *owner_);
    sheet_->SetMediaQueries(MediaQuerySet::Create(owner_->Media()));
    if (owner_->IsInDocumentTree())
      SetSheetTitle(owner_->title());
    SetCrossOriginStylesheetStatus(sheet_.Get());

    loading_ = false;
    restored_sheet->CheckLoaded();

    return;
  }

  StyleSheetContents* style_sheet =
      StyleSheetContents::Create(href, parser_context);

  if (sheet_)
    ClearSheet();

  sheet_ = CSSStyleSheet::Create(style_sheet, *owner_);
  sheet_->SetMediaQueries(MediaQuerySet::Create(owner_->Media()));
  if (owner_->IsInDocumentTree())
    SetSheetTitle(owner_->title());
  SetCrossOriginStylesheetStatus(sheet_.Get());

  style_sheet->ParseAuthorStyleSheet(cached_style_sheet,
                                     owner_->GetDocument().GetSecurityOrigin());

  loading_ = false;
  style_sheet->NotifyLoadedSheet(cached_style_sheet);
  style_sheet->CheckLoaded();

  if (style_sheet->IsCacheableForResource()) {
    const_cast<CSSStyleSheetResource*>(cached_style_sheet)
        ->SaveParsedStyleSheet(style_sheet);
  }
  ClearResource();
}

bool LinkStyle::SheetLoaded() {
  if (!StyleSheetIsLoading()) {
    RemovePendingSheet();
    return true;
  }
  return false;
}

void LinkStyle::NotifyLoadedSheetAndAllCriticalSubresources(
    Node::LoadedSheetErrorStatus error_status) {
  if (fired_load_)
    return;
  loaded_sheet_ = (error_status == Node::kNoErrorLoadingSubresource);
  if (owner_)
    owner_->ScheduleEvent();
  fired_load_ = true;
}

void LinkStyle::StartLoadingDynamicSheet() {
  DCHECK_LT(pending_sheet_type_, kBlocking);
  AddPendingSheet(kBlocking);
}

void LinkStyle::ClearSheet() {
  DCHECK(sheet_);
  DCHECK_EQ(sheet_->ownerNode(), owner_);
  sheet_.Release()->ClearOwnerNode();
}

bool LinkStyle::StyleSheetIsLoading() const {
  if (loading_)
    return true;
  if (!sheet_)
    return false;
  return sheet_->Contents()->IsLoading();
}

void LinkStyle::AddPendingSheet(PendingSheetType type) {
  if (type <= pending_sheet_type_)
    return;
  pending_sheet_type_ = type;

  if (pending_sheet_type_ == kNonBlocking)
    return;
  owner_->GetDocument().GetStyleEngine().AddPendingSheet(style_engine_context_);
}

void LinkStyle::RemovePendingSheet() {
  DCHECK(owner_);
  PendingSheetType type = pending_sheet_type_;
  pending_sheet_type_ = kNone;

  if (type == kNone)
    return;
  if (type == kNonBlocking) {
    // Tell StyleEngine to re-compute styleSheets of this m_owner's treescope.
    owner_->GetDocument().GetStyleEngine().ModifiedStyleSheetCandidateNode(
        *owner_);
    return;
  }

  owner_->GetDocument().GetStyleEngine().RemovePendingSheet(
      *owner_, style_engine_context_);
}

void LinkStyle::SetDisabledState(bool disabled) {
  LinkStyle::DisabledState old_disabled_state = disabled_state_;
  disabled_state_ = disabled ? kDisabled : kEnabledViaScript;
  if (old_disabled_state == disabled_state_)
    return;

  // If we change the disabled state while the sheet is still loading, then we
  // have to perform three checks:
  if (StyleSheetIsLoading()) {
    // Check #1: The sheet becomes disabled while loading.
    if (disabled_state_ == kDisabled)
      RemovePendingSheet();

    // Check #2: An alternate sheet becomes enabled while it is still loading.
    if (owner_->RelAttribute().IsAlternate() &&
        disabled_state_ == kEnabledViaScript)
      AddPendingSheet(kBlocking);

    // Check #3: A main sheet becomes enabled while it was still loading and
    // after it was disabled via script. It takes really terrible code to make
    // this happen (a double toggle for no reason essentially). This happens
    // on virtualplastic.net, which manages to do about 12 enable/disables on
    // only 3 sheets. :)
    if (!owner_->RelAttribute().IsAlternate() &&
        disabled_state_ == kEnabledViaScript && old_disabled_state == kDisabled)
      AddPendingSheet(kBlocking);

    // If the sheet is already loading just bail.
    return;
  }

  if (sheet_) {
    sheet_->setDisabled(disabled);
    return;
  }

  if (disabled_state_ == kEnabledViaScript && owner_->ShouldProcessStyle())
    Process();
}

void LinkStyle::SetCrossOriginStylesheetStatus(CSSStyleSheet* sheet) {
  if (fetch_following_cors_ && GetResource() &&
      !GetResource()->ErrorOccurred()) {
    // Record the security origin the CORS access check succeeded at, if cross
    // origin.  Only origins that are script accessible to it may access the
    // stylesheet's rules.
    sheet->SetAllowRuleAccessFromOrigin(
        owner_->GetDocument().GetSecurityOrigin());
  }
  fetch_following_cors_ = false;
}

// TODO(yoav): move that logic to LinkLoader
LinkStyle::LoadReturnValue LinkStyle::LoadStylesheetIfNeeded(
    const LinkRequestBuilder& builder,
    const String& type) {
  if (disabled_state_ == kDisabled || !owner_->RelAttribute().IsStyleSheet() ||
      !StyleSheetTypeIsSupported(type) || !ShouldLoadResource() ||
      !builder.Url().IsValid())
    return kNotNeeded;

  if (GetResource()) {
    RemovePendingSheet();
    ClearResource();
    ClearFetchFollowingCORS();
  }

  if (!owner_->ShouldLoadLink())
    return kBail;

  loading_ = true;

  String title = owner_->title();
  if (!title.IsEmpty() && !owner_->IsAlternate() &&
      disabled_state_ != kEnabledViaScript && owner_->IsInDocumentTree()) {
    GetDocument().GetStyleEngine().SetPreferredStylesheetSetNameIfNotSet(title);
  }

  bool media_query_matches = true;
  LocalFrame* frame = LoadingFrame();
  if (!owner_->Media().IsEmpty() && frame) {
    RefPtr<MediaQuerySet> media = MediaQuerySet::Create(owner_->Media());
    MediaQueryEvaluator evaluator(frame);
    media_query_matches = evaluator.Eval(*media);
  }

  // Don't hold up layout tree construction and script execution on
  // stylesheets that are not needed for the layout at the moment.
  bool blocking = media_query_matches && !owner_->IsAlternate() &&
                  owner_->IsCreatedByParser();
  AddPendingSheet(blocking ? kBlocking : kNonBlocking);

  // Load stylesheets that are not needed for the layout immediately with low
  // priority.  When the link element is created by scripts, load the
  // stylesheets asynchronously but in high priority.
  bool low_priority = !media_query_matches || owner_->IsAlternate();
  FetchParameters params = builder.Build(low_priority);
  CrossOriginAttributeValue cross_origin = GetCrossOriginAttributeValue(
      owner_->FastGetAttribute(HTMLNames::crossoriginAttr));
  if (cross_origin != kCrossOriginAttributeNotSet) {
    params.SetCrossOriginAccessControl(GetDocument().GetSecurityOrigin(),
                                       cross_origin);
    SetFetchFollowingCORS();
  }

  String integrity_attr = owner_->FastGetAttribute(HTMLNames::integrityAttr);
  if (!integrity_attr.IsEmpty()) {
    IntegrityMetadataSet metadata_set;
    SubresourceIntegrity::ParseIntegrityAttribute(integrity_attr, metadata_set);
    params.SetIntegrityMetadata(metadata_set);
  }
  SetResource(CSSStyleSheetResource::Fetch(params, GetDocument().Fetcher()));

  if (loading_ && !GetResource()) {
    // The request may have been denied if (for example) the stylesheet is
    // local and the document is remote, or if there was a Content Security
    // Policy Failure.  setCSSStyleSheet() can be called synchronuosly in
    // setResource() and thus resource() is null and |m_loading| is false in
    // such cases even if the request succeeds.
    loading_ = false;
    RemovePendingSheet();
    NotifyLoadedSheetAndAllCriticalSubresources(
        Node::kErrorOccurredLoadingSubresource);
  }
  return kLoaded;
}

void LinkStyle::Process() {
  DCHECK(owner_->ShouldProcessStyle());
  String type = owner_->TypeValue().DeprecatedLower();
  String as = owner_->AsValue().DeprecatedLower();
  String media = owner_->Media().DeprecatedLower();
  LinkRequestBuilder builder(owner_);

  if (owner_->RelAttribute().GetIconType() != kInvalidIcon &&
      builder.Url().IsValid() && !builder.Url().IsEmpty()) {
    if (!owner_->ShouldLoadLink())
      return;
    if (!GetDocument().GetSecurityOrigin()->CanDisplay(builder.Url()))
      return;
    if (!GetDocument().GetContentSecurityPolicy()->AllowImageFromSource(
            builder.Url()))
      return;
    if (GetDocument().GetFrame() &&
        GetDocument().GetFrame()->Loader().Client()) {
      GetDocument().GetFrame()->Loader().Client()->DispatchDidChangeIcons(
          owner_->RelAttribute().GetIconType());
    }
  }

  if (!owner_->LoadLink(type, as, media, owner_->GetReferrerPolicy(),
                        builder.Url()))
    return;

  if (LoadStylesheetIfNeeded(builder, type) == kNotNeeded && sheet_) {
    // we no longer contain a stylesheet, e.g. perhaps rel or type was changed
    ClearSheet();
    GetDocument().GetStyleEngine().SetNeedsActiveStyleUpdate(
        owner_->GetTreeScope());
  }
}

void LinkStyle::SetSheetTitle(const String& title) {
  if (!owner_->IsInDocumentTree() || !owner_->RelAttribute().IsStyleSheet())
    return;

  if (sheet_)
    sheet_->SetTitle(title);

  if (title.IsEmpty() || !IsUnset() || owner_->IsAlternate())
    return;

  const KURL& href = owner_->GetNonEmptyURLAttribute(hrefAttr);
  if (href.IsValid() && !href.IsEmpty())
    GetDocument().GetStyleEngine().SetPreferredStylesheetSetNameIfNotSet(title);
}

void LinkStyle::OwnerRemoved() {
  if (StyleSheetIsLoading())
    RemovePendingSheet();

  if (sheet_)
    ClearSheet();
}

DEFINE_TRACE(LinkStyle) {
  visitor->Trace(sheet_);
  LinkResource::Trace(visitor);
  ResourceOwner<StyleSheetResource>::Trace(visitor);
}

}  // namespace blink
