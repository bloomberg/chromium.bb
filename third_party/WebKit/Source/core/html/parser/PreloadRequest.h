// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PreloadRequest_h
#define PreloadRequest_h

#include <memory>
#include "core/CoreExport.h"
#include "core/dom/Script.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/loader/fetch/ClientHintsPreferences.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/TextPosition.h"

namespace blink {

class Document;

class CORE_EXPORT PreloadRequest {
  USING_FAST_MALLOC(PreloadRequest);

 public:
  enum RequestType {
    kRequestTypePreload,
    kRequestTypePreconnect,
    kRequestTypeLinkRelPreload
  };

  enum ReferrerSource { kDocumentIsReferrer, kBaseUrlIsReferrer };

  // TODO(csharrison): Move the implementation to the cpp file when core/html
  // gets its own testing source set in html/BUILD.gn.
  static std::unique_ptr<PreloadRequest> CreateIfNeeded(
      const String& initiator_name,
      const TextPosition& initiator_position,
      const String& resource_url,
      const KURL& base_url,
      Resource::Type resource_type,
      const ReferrerPolicy referrer_policy,
      ReferrerSource referrer_source,
      ResourceFetcher::IsImageSet is_image_set,
      const FetchParameters::ResourceWidth& resource_width =
          FetchParameters::ResourceWidth(),
      const ClientHintsPreferences& client_hints_preferences =
          ClientHintsPreferences(),
      RequestType request_type = kRequestTypePreload) {
    // Never preload data URLs. We also disallow relative ref URLs which become
    // data URLs if the document's URL is a data URL. We don't want to create
    // extra resource requests with data URLs to avoid copy / initialization
    // overhead, which can be significant for large URLs.
    if (resource_url.IsEmpty() || resource_url.StartsWith("#") ||
        ProtocolIs(resource_url, "data")) {
      return nullptr;
    }
    return WTF::WrapUnique(new PreloadRequest(
        initiator_name, initiator_position, resource_url, base_url,
        resource_type, resource_width, client_hints_preferences, request_type,
        referrer_policy, referrer_source, is_image_set));
  }

  bool IsSafeToSendToAnotherThread() const;

  Resource* Start(Document*);

  void SetDefer(FetchParameters::DeferOption defer) { defer_ = defer; }
  void SetCharset(const String& charset) { charset_ = charset.IsolatedCopy(); }
  void SetCrossOrigin(CrossOriginAttributeValue cross_origin) {
    cross_origin_ = cross_origin;
  }
  CrossOriginAttributeValue CrossOrigin() const { return cross_origin_; }

  void SetNonce(const String& nonce) { nonce_ = nonce.IsolatedCopy(); }
  const String& Nonce() const { return nonce_; }

  Resource::Type ResourceType() const { return resource_type_; }

  const String& ResourceURL() const { return resource_url_; }
  float ResourceWidth() const {
    return resource_width_.is_set ? resource_width_.width : 0;
  }
  const KURL& BaseURL() const { return base_url_; }
  bool IsPreconnect() const { return request_type_ == kRequestTypePreconnect; }
  bool IsLinkRelPreload() const {
    return request_type_ == kRequestTypeLinkRelPreload;
  }
  const ClientHintsPreferences& Preferences() const {
    return client_hints_preferences_;
  }
  ReferrerPolicy GetReferrerPolicy() const { return referrer_policy_; }

  void SetScriptType(ScriptType script_type) { script_type_ = script_type; }

  // Only scripts and css stylesheets need to have integrity set on preloads.
  // This is because neither resource keeps raw data around to redo an
  // integrity check. A resource in memory cache needs integrity
  // data cached to match an outgoing request.
  void SetIntegrityMetadata(const IntegrityMetadataSet& metadata_set) {
    integrity_metadata_ = metadata_set;
  }
  void SetFromInsertionScanner(const bool from_insertion_scanner) {
    from_insertion_scanner_ = from_insertion_scanner;
  }

  bool IsImageSetForTestingOnly() const {
    return is_image_set_ == ResourceFetcher::kImageIsImageSet;
  }

 private:
  PreloadRequest(const String& initiator_name,
                 const TextPosition& initiator_position,
                 const String& resource_url,
                 const KURL& base_url,
                 Resource::Type resource_type,
                 const FetchParameters::ResourceWidth& resource_width,
                 const ClientHintsPreferences& client_hints_preferences,
                 RequestType request_type,
                 const ReferrerPolicy referrer_policy,
                 ReferrerSource referrer_source,
                 ResourceFetcher::IsImageSet is_image_set)
      : initiator_name_(initiator_name),
        initiator_position_(initiator_position),
        resource_url_(resource_url.IsolatedCopy()),
        base_url_(base_url.Copy()),
        resource_type_(resource_type),
        script_type_(ScriptType::kClassic),
        cross_origin_(kCrossOriginAttributeNotSet),
        discovery_time_(MonotonicallyIncreasingTime()),
        defer_(FetchParameters::kNoDefer),
        resource_width_(resource_width),
        client_hints_preferences_(client_hints_preferences),
        request_type_(request_type),
        referrer_policy_(referrer_policy),
        referrer_source_(referrer_source),
        from_insertion_scanner_(false),
        is_image_set_(is_image_set) {}

  KURL CompleteURL(Document*);

  String initiator_name_;
  TextPosition initiator_position_;
  String resource_url_;
  KURL base_url_;
  String charset_;
  Resource::Type resource_type_;
  ScriptType script_type_;
  CrossOriginAttributeValue cross_origin_;
  String nonce_;
  double discovery_time_;
  FetchParameters::DeferOption defer_;
  FetchParameters::ResourceWidth resource_width_;
  ClientHintsPreferences client_hints_preferences_;
  RequestType request_type_;
  ReferrerPolicy referrer_policy_;
  ReferrerSource referrer_source_;
  IntegrityMetadataSet integrity_metadata_;
  bool from_insertion_scanner_;
  ResourceFetcher::IsImageSet is_image_set_;
};

typedef Vector<std::unique_ptr<PreloadRequest>> PreloadRequestStream;

}  // namespace blink

#endif
