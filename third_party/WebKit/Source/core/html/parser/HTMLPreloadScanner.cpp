/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/parser/HTMLPreloadScanner.h"

#include <memory>
#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/MediaValuesCached.h"
#include "core/css/parser/SizesAttributeParser.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptLoader.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/SubresourceIntegrity.h"
#include "core/html/CrossOriginAttribute.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/html/LinkRelAttribute.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/parser/HTMLSrcsetParser.h"
#include "core/html/parser/HTMLTokenizer.h"
#include "core/loader/LinkLoader.h"
#include "platform/Histogram.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/network/mime/ContentType.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/wtf/Optional.h"

namespace blink {

namespace {

// When adding values to this enum, update histograms.xml as well.
enum DocumentWriteGatedEvaluation {
  kGatedEvaluationScriptTooLong,
  kGatedEvaluationNoLikelyScript,
  kGatedEvaluationLooping,
  kGatedEvaluationPopularLibrary,
  kGatedEvaluationNondeterminism,

  // Add new values before this last value.
  kGatedEvaluationLastValue
};

void LogGatedEvaluation(DocumentWriteGatedEvaluation reason) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, gated_evaluation_histogram,
                      ("PreloadScanner.DocumentWrite.GatedEvaluation",
                       kGatedEvaluationLastValue));
  gated_evaluation_histogram.Count(reason);
}

}  // namespace

using namespace HTMLNames;

static bool Match(const StringImpl* impl, const QualifiedName& q_name) {
  return impl == q_name.LocalName().Impl();
}

static bool Match(const AtomicString& name, const QualifiedName& q_name) {
  DCHECK(IsMainThread());
  return q_name.LocalName() == name;
}

static bool Match(const String& name, const QualifiedName& q_name) {
  return ThreadSafeMatch(name, q_name);
}

static const StringImpl* TagImplFor(const HTMLToken::DataVector& data) {
  AtomicString tag_name(data);
  const StringImpl* result = tag_name.Impl();
  if (result->IsStatic())
    return result;
  return nullptr;
}

static const StringImpl* TagImplFor(const String& tag_name) {
  const StringImpl* result = tag_name.Impl();
  if (result->IsStatic())
    return result;
  return nullptr;
}

static String InitiatorFor(const StringImpl* tag_impl) {
  DCHECK(tag_impl);
  if (Match(tag_impl, imgTag))
    return imgTag.LocalName();
  if (Match(tag_impl, inputTag))
    return inputTag.LocalName();
  if (Match(tag_impl, linkTag))
    return linkTag.LocalName();
  if (Match(tag_impl, scriptTag))
    return scriptTag.LocalName();
  if (Match(tag_impl, videoTag))
    return videoTag.LocalName();
  NOTREACHED();
  return g_empty_string;
}

static bool MediaAttributeMatches(const MediaValuesCached& media_values,
                                  const String& attribute_value) {
  RefPtr<MediaQuerySet> media_queries = MediaQuerySet::Create(attribute_value);
  MediaQueryEvaluator media_query_evaluator(media_values);
  return media_query_evaluator.Eval(*media_queries);
}

class TokenPreloadScanner::StartTagScanner {
  STACK_ALLOCATED();

 public:
  StartTagScanner(const StringImpl* tag_impl,
                  MediaValuesCached* media_values,
                  TokenPreloadScanner::ScannerType scanner_type)
      : tag_impl_(tag_impl),
        link_is_style_sheet_(false),
        link_is_preconnect_(false),
        link_is_preload_(false),
        link_is_import_(false),
        matched_(true),
        input_is_image_(false),
        nomodule_attribute_value_(false),
        source_size_(0),
        source_size_set_(false),
        defer_(FetchParameters::kNoDefer),
        cross_origin_(kCrossOriginAttributeNotSet),
        media_values_(media_values),
        referrer_policy_set_(false),
        referrer_policy_(kReferrerPolicyDefault),
        scanner_type_(scanner_type) {
    if (Match(tag_impl_, imgTag) || Match(tag_impl_, sourceTag)) {
      source_size_ = SizesAttributeParser(media_values_, String()).length();
      return;
    }
    if (!Match(tag_impl_, inputTag) && !Match(tag_impl_, linkTag) &&
        !Match(tag_impl_, scriptTag) && !Match(tag_impl_, videoTag))
      tag_impl_ = 0;
  }

  enum URLReplacement { kAllowURLReplacement, kDisallowURLReplacement };

  void ProcessAttributes(const HTMLToken::AttributeList& attributes) {
    DCHECK(IsMainThread());
    if (!tag_impl_)
      return;
    for (const HTMLToken::Attribute& html_token_attribute : attributes) {
      AtomicString attribute_name(html_token_attribute.GetName());
      String attribute_value = html_token_attribute.Value8BitIfNecessary();
      ProcessAttribute(attribute_name, attribute_value);
    }
  }

  void ProcessAttributes(
      const Vector<CompactHTMLToken::Attribute>& attributes) {
    if (!tag_impl_)
      return;
    for (const CompactHTMLToken::Attribute& html_token_attribute : attributes)
      ProcessAttribute(html_token_attribute.GetName(),
                       html_token_attribute.Value());
  }

  void HandlePictureSourceURL(PictureData& picture_data) {
    if (Match(tag_impl_, sourceTag) && matched_ &&
        picture_data.source_url.IsEmpty()) {
      // Must create an isolatedCopy() since the srcset attribute value will get
      // sent back to the main thread between when we set this, and when we
      // process the closing tag which would clear m_pictureData. Having any ref
      // to a string we're going to send will fail
      // isSafeToSendToAnotherThread().
      picture_data.source_url =
          srcset_image_candidate_.ToString().IsolatedCopy();
      picture_data.source_size_set = source_size_set_;
      picture_data.source_size = source_size_;
      picture_data.picked = true;
    } else if (Match(tag_impl_, imgTag) && !picture_data.source_url.IsEmpty()) {
      SetUrlToLoad(picture_data.source_url, kAllowURLReplacement);
    }
  }

  std::unique_ptr<PreloadRequest> CreatePreloadRequest(
      const KURL& predicted_base_url,
      const SegmentedString& source,
      const ClientHintsPreferences& client_hints_preferences,
      const PictureData& picture_data,
      const ReferrerPolicy document_referrer_policy) {
    PreloadRequest::RequestType request_type =
        PreloadRequest::kRequestTypePreload;
    WTF::Optional<Resource::Type> type;
    if (ShouldPreconnect()) {
      request_type = PreloadRequest::kRequestTypePreconnect;
    } else {
      if (IsLinkRelPreload()) {
        request_type = PreloadRequest::kRequestTypeLinkRelPreload;
        type = ResourceTypeForLinkPreload();
        if (type == WTF::nullopt)
          return nullptr;
      }
      if (!ShouldPreload(type)) {
        return nullptr;
      }
    }

    TextPosition position =
        TextPosition(source.CurrentLine(), source.CurrentColumn());
    FetchParameters::ResourceWidth resource_width;
    float source_size = source_size_;
    bool source_size_set = source_size_set_;
    if (picture_data.picked) {
      source_size_set = picture_data.source_size_set;
      source_size = picture_data.source_size;
    }
    ResourceFetcher::IsImageSet is_image_set =
        (picture_data.picked || !srcset_image_candidate_.IsEmpty())
            ? ResourceFetcher::kImageIsImageSet
            : ResourceFetcher::kImageNotImageSet;

    if (source_size_set) {
      resource_width.width = source_size;
      resource_width.is_set = true;
    }

    if (type == WTF::nullopt)
      type = ResourceType();

    // The element's 'referrerpolicy' attribute (if present) takes precedence
    // over the document's referrer policy.
    ReferrerPolicy referrer_policy =
        (referrer_policy_ != kReferrerPolicyDefault) ? referrer_policy_
                                                     : document_referrer_policy;
    auto request = PreloadRequest::CreateIfNeeded(
        InitiatorFor(tag_impl_), position, url_to_load_, predicted_base_url,
        type.value(), referrer_policy, PreloadRequest::kDocumentIsReferrer,
        is_image_set, resource_width, client_hints_preferences, request_type);
    if (!request)
      return nullptr;

    request->SetCrossOrigin(cross_origin_);
    request->SetNonce(nonce_);
    request->SetCharset(Charset());
    request->SetDefer(defer_);
    request->SetIntegrityMetadata(integrity_metadata_);
    if (scanner_type_ == ScannerType::kInsertion)
      request->SetFromInsertionScanner(true);

    return request;
  }

 private:
  template <typename NameType>
  void ProcessScriptAttribute(const NameType& attribute_name,
                              const String& attribute_value) {
    // FIXME - Don't set crossorigin multiple times.
    if (Match(attribute_name, srcAttr))
      SetUrlToLoad(attribute_value, kDisallowURLReplacement);
    else if (Match(attribute_name, crossoriginAttr))
      SetCrossOrigin(attribute_value);
    else if (Match(attribute_name, nonceAttr))
      SetNonce(attribute_value);
    else if (Match(attribute_name, asyncAttr))
      SetDefer(FetchParameters::kLazyLoad);
    else if (Match(attribute_name, deferAttr))
      SetDefer(FetchParameters::kLazyLoad);
    // Note that only scripts need to have the integrity metadata set on
    // preloads. This is because script resources fetches, and only script
    // resource fetches, need to re-request resources if a cached version has
    // different metadata (including empty) from the metadata on the request.
    // See the comment before the call to mustRefetchDueToIntegrityMismatch() in
    // Source/core/fetch/ResourceFetcher.cpp for a more complete explanation.
    else if (Match(attribute_name, integrityAttr))
      SubresourceIntegrity::ParseIntegrityAttribute(attribute_value,
                                                    integrity_metadata_);
    else if (Match(attribute_name, typeAttr))
      type_attribute_value_ = attribute_value;
    else if (Match(attribute_name, languageAttr))
      language_attribute_value_ = attribute_value;
    else if (Match(attribute_name, nomoduleAttr))
      nomodule_attribute_value_ = true;
  }

  template <typename NameType>
  void ProcessImgAttribute(const NameType& attribute_name,
                           const String& attribute_value) {
    if (Match(attribute_name, srcAttr) && img_src_url_.IsNull()) {
      img_src_url_ = attribute_value;
      SetUrlToLoad(BestFitSourceForImageAttributes(
                       media_values_->DevicePixelRatio(), source_size_,
                       attribute_value, srcset_image_candidate_),
                   kAllowURLReplacement);
    } else if (Match(attribute_name, crossoriginAttr)) {
      SetCrossOrigin(attribute_value);
    } else if (Match(attribute_name, srcsetAttr) &&
               srcset_image_candidate_.IsEmpty()) {
      srcset_attribute_value_ = attribute_value;
      srcset_image_candidate_ = BestFitSourceForSrcsetAttribute(
          media_values_->DevicePixelRatio(), source_size_, attribute_value);
      SetUrlToLoad(BestFitSourceForImageAttributes(
                       media_values_->DevicePixelRatio(), source_size_,
                       img_src_url_, srcset_image_candidate_),
                   kAllowURLReplacement);
    } else if (Match(attribute_name, sizesAttr) && !source_size_set_) {
      source_size_ =
          SizesAttributeParser(media_values_, attribute_value).length();
      source_size_set_ = true;
      if (!srcset_image_candidate_.IsEmpty()) {
        srcset_image_candidate_ = BestFitSourceForSrcsetAttribute(
            media_values_->DevicePixelRatio(), source_size_,
            srcset_attribute_value_);
        SetUrlToLoad(BestFitSourceForImageAttributes(
                         media_values_->DevicePixelRatio(), source_size_,
                         img_src_url_, srcset_image_candidate_),
                     kAllowURLReplacement);
      }
    } else if (!referrer_policy_set_ &&
               Match(attribute_name, referrerpolicyAttr) &&
               !attribute_value.IsNull()) {
      referrer_policy_set_ = true;
      SecurityPolicy::ReferrerPolicyFromString(
          attribute_value, kSupportReferrerPolicyLegacyKeywords,
          &referrer_policy_);
    }
  }

  template <typename NameType>
  void ProcessLinkAttribute(const NameType& attribute_name,
                            const String& attribute_value) {
    // FIXME - Don't set rel/media/crossorigin multiple times.
    if (Match(attribute_name, hrefAttr)) {
      SetUrlToLoad(attribute_value, kDisallowURLReplacement);
    } else if (Match(attribute_name, relAttr)) {
      LinkRelAttribute rel(attribute_value);
      link_is_style_sheet_ = rel.IsStyleSheet() && !rel.IsAlternate() &&
                             rel.GetIconType() == kInvalidIcon &&
                             !rel.IsDNSPrefetch();
      link_is_preconnect_ = rel.IsPreconnect();
      link_is_preload_ = rel.IsLinkPreload();
      link_is_import_ = rel.IsImport();
    } else if (Match(attribute_name, mediaAttr)) {
      matched_ &= MediaAttributeMatches(*media_values_, attribute_value);
    } else if (Match(attribute_name, crossoriginAttr)) {
      SetCrossOrigin(attribute_value);
    } else if (Match(attribute_name, nonceAttr)) {
      SetNonce(attribute_value);
    } else if (Match(attribute_name, asAttr)) {
      as_attribute_value_ = attribute_value.DeprecatedLower();
    } else if (Match(attribute_name, typeAttr)) {
      type_attribute_value_ = attribute_value;
    } else if (!referrer_policy_set_ &&
               Match(attribute_name, referrerpolicyAttr) &&
               !attribute_value.IsNull()) {
      referrer_policy_set_ = true;
      SecurityPolicy::ReferrerPolicyFromString(
          attribute_value, kDoNotSupportReferrerPolicyLegacyKeywords,
          &referrer_policy_);
    }
  }

  template <typename NameType>
  void ProcessInputAttribute(const NameType& attribute_name,
                             const String& attribute_value) {
    // FIXME - Don't set type multiple times.
    if (Match(attribute_name, srcAttr)) {
      SetUrlToLoad(attribute_value, kDisallowURLReplacement);
    } else if (Match(attribute_name, typeAttr)) {
      input_is_image_ =
          DeprecatedEqualIgnoringCase(attribute_value, InputTypeNames::image);
    }
  }

  template <typename NameType>
  void ProcessSourceAttribute(const NameType& attribute_name,
                              const String& attribute_value) {
    if (Match(attribute_name, srcsetAttr) &&
        srcset_image_candidate_.IsEmpty()) {
      srcset_attribute_value_ = attribute_value;
      srcset_image_candidate_ = BestFitSourceForSrcsetAttribute(
          media_values_->DevicePixelRatio(), source_size_, attribute_value);
    } else if (Match(attribute_name, sizesAttr) && !source_size_set_) {
      source_size_ =
          SizesAttributeParser(media_values_, attribute_value).length();
      source_size_set_ = true;
      if (!srcset_image_candidate_.IsEmpty()) {
        srcset_image_candidate_ = BestFitSourceForSrcsetAttribute(
            media_values_->DevicePixelRatio(), source_size_,
            srcset_attribute_value_);
      }
    } else if (Match(attribute_name, mediaAttr)) {
      // FIXME - Don't match media multiple times.
      matched_ &= MediaAttributeMatches(*media_values_, attribute_value);
    } else if (Match(attribute_name, typeAttr)) {
      matched_ &= MIMETypeRegistry::IsSupportedImagePrefixedMIMEType(
          ContentType(attribute_value).GetType());
    }
  }

  template <typename NameType>
  void ProcessVideoAttribute(const NameType& attribute_name,
                             const String& attribute_value) {
    if (Match(attribute_name, posterAttr))
      SetUrlToLoad(attribute_value, kDisallowURLReplacement);
    else if (Match(attribute_name, crossoriginAttr))
      SetCrossOrigin(attribute_value);
  }

  template <typename NameType>
  void ProcessAttribute(const NameType& attribute_name,
                        const String& attribute_value) {
    if (Match(attribute_name, charsetAttr))
      charset_ = attribute_value;

    if (Match(tag_impl_, scriptTag))
      ProcessScriptAttribute(attribute_name, attribute_value);
    else if (Match(tag_impl_, imgTag))
      ProcessImgAttribute(attribute_name, attribute_value);
    else if (Match(tag_impl_, linkTag))
      ProcessLinkAttribute(attribute_name, attribute_value);
    else if (Match(tag_impl_, inputTag))
      ProcessInputAttribute(attribute_name, attribute_value);
    else if (Match(tag_impl_, sourceTag))
      ProcessSourceAttribute(attribute_name, attribute_value);
    else if (Match(tag_impl_, videoTag))
      ProcessVideoAttribute(attribute_name, attribute_value);
  }

  void SetUrlToLoad(const String& value, URLReplacement replacement) {
    // We only respect the first src/href, per HTML5:
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#attribute-name-state
    if (replacement == kDisallowURLReplacement && !url_to_load_.IsEmpty())
      return;
    String url = StripLeadingAndTrailingHTMLSpaces(value);
    if (url.IsEmpty())
      return;
    url_to_load_ = url;
  }

  const String& Charset() const {
    // FIXME: Its not clear that this if is needed, the loader probably ignores
    // charset for image requests anyway.
    if (Match(tag_impl_, imgTag) || Match(tag_impl_, videoTag))
      return g_empty_string;
    return charset_;
  }

  WTF::Optional<Resource::Type> ResourceTypeForLinkPreload() const {
    DCHECK(link_is_preload_);
    return LinkLoader::GetResourceTypeFromAsAttribute(as_attribute_value_);
  }

  Resource::Type ResourceType() const {
    if (Match(tag_impl_, scriptTag)) {
      return Resource::kScript;
    } else if (Match(tag_impl_, imgTag) || Match(tag_impl_, videoTag) ||
               (Match(tag_impl_, inputTag) && input_is_image_)) {
      return Resource::kImage;
    } else if (Match(tag_impl_, linkTag) && link_is_style_sheet_) {
      return Resource::kCSSStyleSheet;
    } else if (link_is_preconnect_) {
      return Resource::kRaw;
    } else if (Match(tag_impl_, linkTag) && link_is_import_) {
      return Resource::kImportResource;
    }
    NOTREACHED();
    return Resource::kRaw;
  }

  bool ShouldPreconnect() const {
    return Match(tag_impl_, linkTag) && link_is_preconnect_ &&
           !url_to_load_.IsEmpty();
  }

  bool IsLinkRelPreload() const {
    return Match(tag_impl_, linkTag) && link_is_preload_ &&
           !url_to_load_.IsEmpty();
  }

  bool ShouldPreloadLink(WTF::Optional<Resource::Type>& type) const {
    if (link_is_style_sheet_) {
      return type_attribute_value_.IsEmpty() ||
             MIMETypeRegistry::IsSupportedStyleSheetMIMEType(
                 ContentType(type_attribute_value_).GetType());
    } else if (link_is_preload_) {
      if (type_attribute_value_.IsEmpty())
        return true;
      String type_from_attribute = ContentType(type_attribute_value_).GetType();
      if ((type == Resource::kFont &&
           !MIMETypeRegistry::IsSupportedFontMIMEType(type_from_attribute)) ||
          (type == Resource::kImage &&
           !MIMETypeRegistry::IsSupportedImagePrefixedMIMEType(
               type_from_attribute)) ||
          (type == Resource::kCSSStyleSheet &&
           !MIMETypeRegistry::IsSupportedStyleSheetMIMEType(
               type_from_attribute))) {
        return false;
      }
    } else if (!link_is_import_) {
      return false;
    }

    return true;
  }

  bool ShouldPreload(WTF::Optional<Resource::Type>& type) const {
    if (url_to_load_.IsEmpty())
      return false;
    if (!matched_)
      return false;
    if (Match(tag_impl_, linkTag))
      return ShouldPreloadLink(type);
    if (Match(tag_impl_, inputTag) && !input_is_image_)
      return false;
    if (Match(tag_impl_, scriptTag)) {
      ScriptType script_type = ScriptType::kClassic;
      if (!ScriptLoader::IsValidScriptTypeAndLanguage(
              type_attribute_value_, language_attribute_value_,
              ScriptLoader::kAllowLegacyTypeInTypeAttribute, script_type)) {
        return false;
      }
      // TODO(kouhei): Enable preload for module scripts, with correct
      // credentials mode.
      if (type_attribute_value_ == "module")
        return false;
      if (ScriptLoader::BlockForNoModule(script_type,
                                         nomodule_attribute_value_)) {
        return false;
      }
    }
    return true;
  }

  void SetCrossOrigin(const String& cors_setting) {
    cross_origin_ = GetCrossOriginAttributeValue(cors_setting);
  }

  void SetNonce(const String& nonce) { nonce_ = nonce; }

  void SetDefer(FetchParameters::DeferOption defer) { defer_ = defer; }

  bool Defer() const { return defer_; }

  const StringImpl* tag_impl_;
  String url_to_load_;
  ImageCandidate srcset_image_candidate_;
  String charset_;
  bool link_is_style_sheet_;
  bool link_is_preconnect_;
  bool link_is_preload_;
  bool link_is_import_;
  bool matched_;
  bool input_is_image_;
  String img_src_url_;
  String srcset_attribute_value_;
  String as_attribute_value_;
  String type_attribute_value_;
  String language_attribute_value_;
  bool nomodule_attribute_value_;
  float source_size_;
  bool source_size_set_;
  FetchParameters::DeferOption defer_;
  CrossOriginAttributeValue cross_origin_;
  String nonce_;
  Member<MediaValuesCached> media_values_;
  bool referrer_policy_set_;
  ReferrerPolicy referrer_policy_;
  IntegrityMetadataSet integrity_metadata_;
  TokenPreloadScanner::ScannerType scanner_type_;
};

TokenPreloadScanner::TokenPreloadScanner(
    const KURL& document_url,
    std::unique_ptr<CachedDocumentParameters> document_parameters,
    const MediaValuesCached::MediaValuesCachedData& media_values_cached_data,
    const ScannerType scanner_type)
    : document_url_(document_url),
      in_style_(false),
      in_picture_(false),
      in_script_(false),
      template_count_(0),
      document_parameters_(std::move(document_parameters)),
      media_values_(MediaValuesCached::Create(media_values_cached_data)),
      scanner_type_(scanner_type),
      did_rewind_(false) {
  DCHECK(document_parameters_.get());
  DCHECK(media_values_.Get());
  DCHECK(document_url.IsValid());
  css_scanner_.SetReferrerPolicy(document_parameters_->referrer_policy);
}

TokenPreloadScanner::~TokenPreloadScanner() {}

TokenPreloadScannerCheckpoint TokenPreloadScanner::CreateCheckpoint() {
  TokenPreloadScannerCheckpoint checkpoint = checkpoints_.size();
  checkpoints_.push_back(Checkpoint(predicted_base_element_url_, in_style_,
                                    in_script_, template_count_));
  return checkpoint;
}

void TokenPreloadScanner::RewindTo(
    TokenPreloadScannerCheckpoint checkpoint_index) {
  // If this ASSERT fires, checkpointIndex is invalid.
  DCHECK_LT(checkpoint_index, checkpoints_.size());
  const Checkpoint& checkpoint = checkpoints_[checkpoint_index];
  predicted_base_element_url_ = checkpoint.predicted_base_element_url;
  in_style_ = checkpoint.in_style;
  template_count_ = checkpoint.template_count;

  did_rewind_ = true;
  in_script_ = checkpoint.in_script;

  css_scanner_.Reset();
  checkpoints_.clear();
}

void TokenPreloadScanner::Scan(const HTMLToken& token,
                               const SegmentedString& source,
                               PreloadRequestStream& requests,
                               ViewportDescriptionWrapper* viewport,
                               bool* is_csp_meta_tag) {
  ScanCommon(token, source, requests, viewport, is_csp_meta_tag, nullptr);
}

void TokenPreloadScanner::Scan(const CompactHTMLToken& token,
                               const SegmentedString& source,
                               PreloadRequestStream& requests,
                               ViewportDescriptionWrapper* viewport,
                               bool* is_csp_meta_tag,
                               bool* likely_document_write_script) {
  ScanCommon(token, source, requests, viewport, is_csp_meta_tag,
             likely_document_write_script);
}

static void HandleMetaViewport(
    const String& attribute_value,
    const CachedDocumentParameters* document_parameters,
    MediaValuesCached* media_values,
    ViewportDescriptionWrapper* viewport) {
  if (!document_parameters->viewport_meta_enabled)
    return;
  ViewportDescription description(ViewportDescription::kViewportMeta);
  HTMLMetaElement::GetViewportDescriptionFromContentAttribute(
      attribute_value, description, nullptr,
      document_parameters->viewport_meta_zero_values_quirk);
  if (viewport) {
    viewport->description = description;
    viewport->set = true;
  }
  FloatSize initial_viewport(media_values->DeviceWidth(),
                             media_values->DeviceHeight());
  PageScaleConstraints constraints = description.Resolve(
      initial_viewport, document_parameters->default_viewport_min_width);
  media_values->OverrideViewportDimensions(constraints.layout_size.Width(),
                                           constraints.layout_size.Height());
}

static void HandleMetaReferrer(const String& attribute_value,
                               CachedDocumentParameters* document_parameters,
                               CSSPreloadScanner* css_scanner) {
  ReferrerPolicy meta_referrer_policy = kReferrerPolicyDefault;
  if (!attribute_value.IsEmpty() && !attribute_value.IsNull() &&
      SecurityPolicy::ReferrerPolicyFromString(
          attribute_value, kSupportReferrerPolicyLegacyKeywords,
          &meta_referrer_policy)) {
    document_parameters->referrer_policy = meta_referrer_policy;
  }
  css_scanner->SetReferrerPolicy(document_parameters->referrer_policy);
}

template <typename Token>
static void HandleMetaNameAttribute(
    const Token& token,
    CachedDocumentParameters* document_parameters,
    MediaValuesCached* media_values,
    CSSPreloadScanner* css_scanner,
    ViewportDescriptionWrapper* viewport) {
  const typename Token::Attribute* name_attribute =
      token.GetAttributeItem(nameAttr);
  if (!name_attribute)
    return;

  String name_attribute_value(name_attribute->Value());
  const typename Token::Attribute* content_attribute =
      token.GetAttributeItem(contentAttr);
  if (!content_attribute)
    return;

  String content_attribute_value(content_attribute->Value());
  if (DeprecatedEqualIgnoringCase(name_attribute_value, "viewport")) {
    HandleMetaViewport(content_attribute_value, document_parameters,
                       media_values, viewport);
    return;
  }

  if (DeprecatedEqualIgnoringCase(name_attribute_value, "referrer")) {
    HandleMetaReferrer(content_attribute_value, document_parameters,
                       css_scanner);
  }
}

// This method returns true for script source strings which will likely use
// document.write to insert an external script. These scripts will be flagged
// for evaluation via the DocumentWriteEvaluator, so it also dismisses scripts
// that will likely fail evaluation. These includes scripts that are too long,
// have looping constructs, or use non-determinism. Note that flagging occurs
// even when the experiment is off, to ensure fair comparison between experiment
// and control groups.
bool TokenPreloadScanner::ShouldEvaluateForDocumentWrite(const String& source) {
  // The maximum length script source that will be marked for evaluation to
  // preload document.written external scripts.
  const int kMaxLengthForEvaluating = 1024;
  if (!document_parameters_->do_document_write_preload_scanning)
    return false;

  if (source.length() > kMaxLengthForEvaluating) {
    LogGatedEvaluation(kGatedEvaluationScriptTooLong);
    return false;
  }
  if (source.Find("document.write") == WTF::kNotFound ||
      source.FindIgnoringASCIICase("src") == WTF::kNotFound) {
    LogGatedEvaluation(kGatedEvaluationNoLikelyScript);
    return false;
  }
  if (source.FindIgnoringASCIICase("<sc") == WTF::kNotFound &&
      source.FindIgnoringASCIICase("%3Csc") == WTF::kNotFound) {
    LogGatedEvaluation(kGatedEvaluationNoLikelyScript);
    return false;
  }
  if (source.Find("while") != WTF::kNotFound ||
      source.Find("for(") != WTF::kNotFound ||
      source.Find("for ") != WTF::kNotFound) {
    LogGatedEvaluation(kGatedEvaluationLooping);
    return false;
  }
  // This check is mostly for "window.jQuery" for false positives fetches,
  // though it include $ calls to avoid evaluations which will quickly fail.
  if (source.Find("jQuery") != WTF::kNotFound ||
      source.Find("$.") != WTF::kNotFound ||
      source.Find("$(") != WTF::kNotFound) {
    LogGatedEvaluation(kGatedEvaluationPopularLibrary);
    return false;
  }
  if (source.Find("Math.random") != WTF::kNotFound ||
      source.Find("Date") != WTF::kNotFound) {
    LogGatedEvaluation(kGatedEvaluationNondeterminism);
    return false;
  }
  return true;
}

template <typename Token>
void TokenPreloadScanner::ScanCommon(const Token& token,
                                     const SegmentedString& source,
                                     PreloadRequestStream& requests,
                                     ViewportDescriptionWrapper* viewport,
                                     bool* is_csp_meta_tag,
                                     bool* likely_document_write_script) {
  if (!document_parameters_->do_html_preload_scanning)
    return;

  switch (token.GetType()) {
    case HTMLToken::kCharacter: {
      if (in_style_) {
        css_scanner_.Scan(token.Data(), source, requests,
                          predicted_base_element_url_);
      } else if (in_script_ && likely_document_write_script && !did_rewind_) {
        // Don't mark scripts for evaluation if the preloader rewound to a
        // previous checkpoint. This could cause re-evaluation of scripts if
        // care isn't given.
        // TODO(csharrison): Revisit this if rewinds are low hanging fruit for
        // the document.write evaluator.
        *likely_document_write_script =
            ShouldEvaluateForDocumentWrite(token.Data());
      }
      return;
    }
    case HTMLToken::kEndTag: {
      const StringImpl* tag_impl = TagImplFor(token.Data());
      if (Match(tag_impl, templateTag)) {
        if (template_count_)
          --template_count_;
        return;
      }
      if (Match(tag_impl, styleTag)) {
        if (in_style_)
          css_scanner_.Reset();
        in_style_ = false;
        return;
      }
      if (Match(tag_impl, scriptTag)) {
        in_script_ = false;
        return;
      }
      if (Match(tag_impl, pictureTag)) {
        in_picture_ = false;
        picture_data_.picked = false;
      }
      return;
    }
    case HTMLToken::kStartTag: {
      if (template_count_)
        return;
      const StringImpl* tag_impl = TagImplFor(token.Data());
      if (Match(tag_impl, templateTag)) {
        ++template_count_;
        return;
      }
      if (Match(tag_impl, styleTag)) {
        in_style_ = true;
        return;
      }
      // Don't early return, because the StartTagScanner needs to look at these
      // too.
      if (Match(tag_impl, scriptTag)) {
        in_script_ = true;
      }
      if (Match(tag_impl, baseTag)) {
        // The first <base> element is the one that wins.
        if (!predicted_base_element_url_.IsEmpty())
          return;
        UpdatePredictedBaseURL(token);
        return;
      }
      if (Match(tag_impl, metaTag)) {
        const typename Token::Attribute* equiv_attribute =
            token.GetAttributeItem(http_equivAttr);
        if (equiv_attribute) {
          String equiv_attribute_value(equiv_attribute->Value());
          if (DeprecatedEqualIgnoringCase(equiv_attribute_value,
                                          "content-security-policy")) {
            *is_csp_meta_tag = true;
          } else if (DeprecatedEqualIgnoringCase(equiv_attribute_value,
                                                 "accept-ch")) {
            const typename Token::Attribute* content_attribute =
                token.GetAttributeItem(contentAttr);
            if (content_attribute)
              client_hints_preferences_.UpdateFromAcceptClientHintsHeader(
                  content_attribute->Value(), nullptr);
          }
          return;
        }

        HandleMetaNameAttribute(token, document_parameters_.get(),
                                media_values_.Get(), &css_scanner_, viewport);
      }

      if (Match(tag_impl, pictureTag)) {
        in_picture_ = true;
        picture_data_ = PictureData();
        return;
      }

      StartTagScanner scanner(tag_impl, media_values_, scanner_type_);
      scanner.ProcessAttributes(token.Attributes());
      // TODO(yoav): ViewportWidth is currently racy and might be zero in some
      // cases, at least in tests. That problem will go away once
      // ParseHTMLOnMainThread lands and MediaValuesCached is eliminated.
      if (in_picture_ && media_values_->ViewportWidth())
        scanner.HandlePictureSourceURL(picture_data_);
      std::unique_ptr<PreloadRequest> request = scanner.CreatePreloadRequest(
          predicted_base_element_url_, source, client_hints_preferences_,
          picture_data_, document_parameters_->referrer_policy);
      if (request)
        requests.push_back(std::move(request));
      return;
    }
    default: { return; }
  }
}

template <typename Token>
void TokenPreloadScanner::UpdatePredictedBaseURL(const Token& token) {
  DCHECK(predicted_base_element_url_.IsEmpty());
  if (const typename Token::Attribute* href_attribute =
          token.GetAttributeItem(hrefAttr)) {
    KURL url(document_url_, StripLeadingAndTrailingHTMLSpaces(
                                href_attribute->Value8BitIfNecessary()));
    predicted_base_element_url_ =
        url.IsValid() && !url.ProtocolIsData() ? url.Copy() : KURL();
  }
}

HTMLPreloadScanner::HTMLPreloadScanner(
    const HTMLParserOptions& options,
    const KURL& document_url,
    std::unique_ptr<CachedDocumentParameters> document_parameters,
    const MediaValuesCached::MediaValuesCachedData& media_values_cached_data,
    const TokenPreloadScanner::ScannerType scanner_type)
    : scanner_(document_url,
               std::move(document_parameters),
               media_values_cached_data,
               scanner_type),
      tokenizer_(HTMLTokenizer::Create(options)) {}

HTMLPreloadScanner::~HTMLPreloadScanner() {}

void HTMLPreloadScanner::AppendToEnd(const SegmentedString& source) {
  source_.Append(source);
}

PreloadRequestStream HTMLPreloadScanner::Scan(
    const KURL& starting_base_element_url,
    ViewportDescriptionWrapper* viewport) {
  // HTMLTokenizer::updateStateFor only works on the main thread.
  DCHECK(IsMainThread());

  TRACE_EVENT1("blink", "HTMLPreloadScanner::scan", "source_length",
               source_.length());

  // When we start scanning, our best prediction of the baseElementURL is the
  // real one!
  if (!starting_base_element_url.IsEmpty())
    scanner_.SetPredictedBaseElementURL(starting_base_element_url);

  PreloadRequestStream requests;

  while (tokenizer_->NextToken(source_, token_)) {
    if (token_.GetType() == HTMLToken::kStartTag)
      tokenizer_->UpdateStateFor(
          AttemptStaticStringCreation(token_.GetName(), kLikely8Bit));
    bool is_csp_meta_tag = false;
    scanner_.Scan(token_, source_, requests, viewport, &is_csp_meta_tag);
    token_.Clear();
    // Don't preload anything if a CSP meta tag is found. We should never really
    // find them here because the HTMLPreloadScanner is only used for
    // dynamically added markup.
    if (is_csp_meta_tag)
      return requests;
  }

  return requests;
}

CachedDocumentParameters::CachedDocumentParameters(Document* document) {
  DCHECK(IsMainThread());
  DCHECK(document);
  do_html_preload_scanning =
      !document->GetSettings() ||
      document->GetSettings()->GetDoHtmlPreloadScanning();
  do_document_write_preload_scanning = do_html_preload_scanning &&
                                       document->GetFrame() &&
                                       document->GetFrame()->IsMainFrame();
  default_viewport_min_width = document->ViewportDefaultMinWidth();
  viewport_meta_zero_values_quirk =
      document->GetSettings() &&
      document->GetSettings()->GetViewportMetaZeroValuesQuirk();
  viewport_meta_enabled = document->GetSettings() &&
                          document->GetSettings()->GetViewportMetaEnabled();
  referrer_policy = document->GetReferrerPolicy();
}

}  // namespace blink
