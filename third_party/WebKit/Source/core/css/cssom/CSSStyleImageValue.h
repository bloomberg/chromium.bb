// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSStyleImageValue_h
#define CSSStyleImageValue_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSResourceValue.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "platform/wtf/Optional.h"

namespace blink {

// CSSStyleImageValue is the base class for Typed OM representations of images.
// The corresponding idl file is CSSImageValue.idl.
class CORE_EXPORT CSSStyleImageValue : public CSSResourceValue,
                                       public CanvasImageSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual ~CSSStyleImageValue() = default;

  // IDL
  double intrinsicWidth(bool& is_null) const;
  double intrinsicHeight(bool& is_null) const;
  double intrinsicRatio(bool& is_null) const;

  // CanvasImageSource
  bool IsCSSImageValue() const final { return true; }
  bool WouldTaintOrigin(
      const SecurityOrigin* destination_security_origin) const final {
    return true;
  }
  FloatSize ElementSize(const FloatSize& default_object_size) const final;

 protected:
  CSSStyleImageValue() = default;

  virtual WTF::Optional<IntSize> IntrinsicSize() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CSSStyleImageValue);
};

}  // namespace blink

#endif  // CSSResourceValue_h
