// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSURIValue_h
#define CSSURIValue_h

#include "core/css/CSSValue.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class KURL;
class SVGElementProxy;

class CSSURIValue : public CSSValue {
 public:
  static CSSURIValue* create(const String& relativeUrl, const KURL& url) {
    return new CSSURIValue(AtomicString(relativeUrl), url);
  }
  static CSSURIValue* create(const AtomicString& absoluteUrl) {
    return new CSSURIValue(absoluteUrl, absoluteUrl);
  }
  ~CSSURIValue();

  SVGElementProxy& ensureElementProxy(const Document&) const;
  void reResolveUrl(const Document&) const;

  const String& value() const { return m_relativeUrl; }

  String customCSSText() const;

  bool isLocal(const Document&) const;
  bool equals(const CSSURIValue&) const;

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  CSSURIValue(const AtomicString&, const KURL&);
  CSSURIValue(const AtomicString& relativeUrl, const AtomicString& absoluteUrl);

  KURL absoluteUrl() const;
  AtomicString fragmentIdentifier() const;

  AtomicString m_relativeUrl;
  bool m_isLocal;

  mutable Member<SVGElementProxy> m_proxy;
  mutable AtomicString m_absoluteUrl;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSURIValue, isURIValue());

}  // namespace blink

#endif  // CSSURIValue_h
