/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef HTMLPreloadScanner_h
#define HTMLPreloadScanner_h

#include <memory>
#include "core/CoreExport.h"
#include "core/css/MediaValuesCached.h"
#include "core/dom/ViewportDescription.h"
#include "core/html/parser/CSSPreloadScanner.h"
#include "core/html/parser/CompactHTMLToken.h"
#include "core/html/parser/HTMLToken.h"
#include "core/html/parser/PreloadRequest.h"
#include "platform/text/SegmentedString.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"

namespace blink {

typedef size_t TokenPreloadScannerCheckpoint;

class HTMLParserOptions;
class HTMLTokenizer;
class SegmentedString;

struct ViewportDescriptionWrapper {
  ViewportDescription description;
  bool set;
  ViewportDescriptionWrapper() : set(false) {}
};

struct CORE_EXPORT CachedDocumentParameters {
  USING_FAST_MALLOC(CachedDocumentParameters);

 public:
  static std::unique_ptr<CachedDocumentParameters> Create(Document* document) {
    return WTF::WrapUnique(new CachedDocumentParameters(document));
  }

  static std::unique_ptr<CachedDocumentParameters> Create() {
    return WTF::WrapUnique(new CachedDocumentParameters);
  }

  bool do_html_preload_scanning;
  Length default_viewport_min_width;
  bool viewport_meta_zero_values_quirk;
  bool viewport_meta_enabled;
  ReferrerPolicy referrer_policy;

 private:
  explicit CachedDocumentParameters(Document*);
  CachedDocumentParameters() = default;
};

class TokenPreloadScanner {
  WTF_MAKE_NONCOPYABLE(TokenPreloadScanner);
  USING_FAST_MALLOC(TokenPreloadScanner);

 public:
  enum class ScannerType { kMainDocument, kInsertion };

  TokenPreloadScanner(const KURL& document_url,
                      std::unique_ptr<CachedDocumentParameters>,
                      const MediaValuesCached::MediaValuesCachedData&,
                      const ScannerType);
  ~TokenPreloadScanner();

  void Scan(const HTMLToken&,
            const SegmentedString&,
            PreloadRequestStream& requests,
            ViewportDescriptionWrapper*,
            bool* is_csp_meta_tag);
  void Scan(const CompactHTMLToken&,
            const SegmentedString&,
            PreloadRequestStream& requests,
            ViewportDescriptionWrapper*,
            bool* is_csp_meta_tag);

  void SetPredictedBaseElementURL(const KURL& url) {
    predicted_base_element_url_ = url;
  }

  // A TokenPreloadScannerCheckpoint is valid until the next call to rewindTo,
  // at which point all outstanding checkpoints are invalidated.
  TokenPreloadScannerCheckpoint CreateCheckpoint();
  void RewindTo(TokenPreloadScannerCheckpoint);

 private:
  class StartTagScanner;

  template <typename Token>
  inline void ScanCommon(const Token&,
                         const SegmentedString&,
                         PreloadRequestStream& requests,
                         ViewportDescriptionWrapper*,
                         bool* is_csp_meta_tag);

  template <typename Token>
  void UpdatePredictedBaseURL(const Token&);

  struct Checkpoint {
    Checkpoint(const KURL& predicted_base_element_url,
               bool in_style,
               bool in_script,
               size_t template_count)
        : predicted_base_element_url(predicted_base_element_url),
          in_style(in_style),
          in_script(in_script),
          template_count(template_count) {}

    KURL predicted_base_element_url;
    bool in_style;
    bool in_script;
    size_t template_count;
  };

  struct PictureData {
    PictureData() : source_size(0.0), source_size_set(false), picked(false) {}
    String source_url;
    float source_size;
    bool source_size_set;
    bool picked;
  };

  CSSPreloadScanner css_scanner_;
  const KURL document_url_;
  KURL predicted_base_element_url_;
  bool in_style_;
  bool in_picture_;
  bool in_script_;
  PictureData picture_data_;
  size_t template_count_;
  std::unique_ptr<CachedDocumentParameters> document_parameters_;
  Persistent<MediaValuesCached> media_values_;
  ClientHintsPreferences client_hints_preferences_;
  ScannerType scanner_type_;

  bool did_rewind_ = false;

  Vector<Checkpoint> checkpoints_;
};

class CORE_EXPORT HTMLPreloadScanner {
  WTF_MAKE_NONCOPYABLE(HTMLPreloadScanner);
  USING_FAST_MALLOC(HTMLPreloadScanner);

 public:
  static std::unique_ptr<HTMLPreloadScanner> Create(
      const HTMLParserOptions& options,
      const KURL& document_url,
      std::unique_ptr<CachedDocumentParameters> document_parameters,
      const MediaValuesCached::MediaValuesCachedData& media_values_cached_data,
      const TokenPreloadScanner::ScannerType scanner_type) {
    return WTF::WrapUnique(new HTMLPreloadScanner(
        options, document_url, std::move(document_parameters),
        media_values_cached_data, scanner_type));
  }

  ~HTMLPreloadScanner();

  void AppendToEnd(const SegmentedString&);
  PreloadRequestStream Scan(const KURL& document_base_element_url,
                            ViewportDescriptionWrapper*);

 private:
  HTMLPreloadScanner(const HTMLParserOptions&,
                     const KURL& document_url,
                     std::unique_ptr<CachedDocumentParameters>,
                     const MediaValuesCached::MediaValuesCachedData&,
                     const TokenPreloadScanner::ScannerType);

  TokenPreloadScanner scanner_;
  SegmentedString source_;
  HTMLToken token_;
  std::unique_ptr<HTMLTokenizer> tokenizer_;
};

}  // namespace blink

#endif
