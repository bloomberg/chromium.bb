// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSURIValue.h"

#include "core/css/CSSMarkup.h"
#include "core/dom/Document.h"
#include "core/svg/SVGElementProxy.h"
#include "platform/weborigin/KURL.h"
#include "wtf/text/WTFString.h"

namespace blink {

CSSURIValue::CSSURIValue(const AtomicString& relativeUrl,
                         const AtomicString& absoluteUrl)
    : CSSValue(URIClass),
      m_relativeUrl(relativeUrl),
      m_isLocal(relativeUrl.startsWith('#')),
      m_absoluteUrl(absoluteUrl) {}

CSSURIValue::CSSURIValue(const AtomicString& relativeUrl, const KURL& url)
    : CSSURIValue(relativeUrl, AtomicString(url.getString())) {}

CSSURIValue::~CSSURIValue() {}

SVGElementProxy& CSSURIValue::ensureElementProxy(
    const Document& document) const {
  if (m_proxy)
    return *m_proxy;
  AtomicString fragmentId = fragmentIdentifier();
  if (isLocal(document))
    m_proxy = SVGElementProxy::create(fragmentId);
  else
    m_proxy = SVGElementProxy::create(m_absoluteUrl, fragmentId);
  return *m_proxy;
}

void CSSURIValue::reResolveUrl(const Document& document) const {
  if (m_isLocal)
    return;
  KURL url = document.completeURL(m_relativeUrl);
  AtomicString urlString(url.getString());
  if (urlString == m_absoluteUrl)
    return;
  m_absoluteUrl = urlString;
  m_proxy = nullptr;
}

String CSSURIValue::customCSSText() const {
  return serializeURI(m_relativeUrl);
}

AtomicString CSSURIValue::fragmentIdentifier() const {
  if (m_isLocal)
    return AtomicString(m_relativeUrl.getString().substring(1));
  return AtomicString(absoluteUrl().fragmentIdentifier());
}

KURL CSSURIValue::absoluteUrl() const {
  return KURL(ParsedURLString, m_absoluteUrl);
}

bool CSSURIValue::isLocal(const Document& document) const {
  return m_isLocal ||
         equalIgnoringFragmentIdentifier(absoluteUrl(), document.url());
}

bool CSSURIValue::equals(const CSSURIValue& other) const {
  // If only one has the 'local url' flag set, the URLs can't match.
  if (m_isLocal != other.m_isLocal)
    return false;
  return (m_isLocal && m_relativeUrl == other.m_relativeUrl) ||
         m_absoluteUrl == other.m_absoluteUrl;
}

DEFINE_TRACE_AFTER_DISPATCH(CSSURIValue) {
  visitor->trace(m_proxy);
  CSSValue::traceAfterDispatch(visitor);
}

}  // namespace blink
