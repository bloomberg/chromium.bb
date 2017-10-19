// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSURIValue_h
#define CSSURIValue_h

#include "core/css/CSSValue.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Document;
class KURL;
class SVGElementProxy;

class CSSURIValue : public CSSValue {
 public:
  static CSSURIValue* Create(const String& relative_url, const KURL& url) {
    return new CSSURIValue(AtomicString(relative_url), url);
  }
  static CSSURIValue* Create(const AtomicString& absolute_url) {
    return new CSSURIValue(absolute_url, absolute_url);
  }
  ~CSSURIValue();

  SVGElementProxy& EnsureElementProxy(const Document&) const;
  void ReResolveUrl(const Document&) const;

  const String& Value() const { return relative_url_; }

  String CustomCSSText() const;

  bool IsLocal(const Document&) const;
  bool Equals(const CSSURIValue&) const;

  void TraceAfterDispatch(blink::Visitor*);

 private:
  CSSURIValue(const AtomicString&, const KURL&);
  CSSURIValue(const AtomicString& relative_url,
              const AtomicString& absolute_url);

  KURL AbsoluteUrl() const;
  AtomicString FragmentIdentifier() const;

  AtomicString relative_url_;
  bool is_local_;

  mutable Member<SVGElementProxy> proxy_;
  mutable AtomicString absolute_url_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSURIValue, IsURIValue());

}  // namespace blink

#endif  // CSSURIValue_h
