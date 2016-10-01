// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSURIValue.h"

#include "core/css/CSSMarkup.h"
#include "core/dom/Document.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceFetcher.h"
#include "wtf/text/WTFString.h"

namespace blink {

CSSURIValue::CSSURIValue(const String& urlString)
    : CSSValue(URIClass), m_url(urlString), m_loadRequested(false) {}

CSSURIValue::~CSSURIValue() {}

DocumentResource* CSSURIValue::load(Document& document) const {
  if (!m_loadRequested) {
    m_loadRequested = true;

    FetchRequest request(ResourceRequest(document.completeURL(m_url)),
                         FetchInitiatorTypeNames::css);
    m_document =
        DocumentResource::fetchSVGDocument(request, document.fetcher());
  }
  return m_document;
}

String CSSURIValue::customCSSText() const {
  return serializeURI(m_url);
}

bool CSSURIValue::equals(const CSSURIValue& other) const {
  return m_url == other.m_url;
}

DEFINE_TRACE_AFTER_DISPATCH(CSSURIValue) {
  visitor->trace(m_document);
  CSSValue::traceAfterDispatch(visitor);
}

}  // namespace blink
