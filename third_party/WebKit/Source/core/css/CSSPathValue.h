// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPathValue_h
#define CSSPathValue_h

#include <memory>
#include "core/css/CSSValue.h"
#include "core/style/StylePath.h"
#include "core/svg/SVGPathByteStream.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class StylePath;

namespace cssvalue {

class CSSPathValue : public CSSValue {
 public:
  static CSSPathValue* Create(PassRefPtr<StylePath>);
  static CSSPathValue* Create(std::unique_ptr<SVGPathByteStream>);

  static CSSPathValue& EmptyPathValue();

  StylePath* GetStylePath() const { return style_path_.Get(); }
  String CustomCSSText() const;

  bool Equals(const CSSPathValue&) const;

  DECLARE_TRACE_AFTER_DISPATCH();

  const SVGPathByteStream& ByteStream() const {
    return style_path_->ByteStream();
  }

 private:
  CSSPathValue(PassRefPtr<StylePath>);

  RefPtr<StylePath> style_path_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSPathValue, IsPathValue());

}  // namespace cssvalue
}  // namespace blink

#endif  // CSSPathValue_h
