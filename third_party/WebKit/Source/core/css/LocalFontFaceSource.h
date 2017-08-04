// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LocalFontFaceSource_h
#define LocalFontFaceSource_h

#include "core/css/CSSFontFaceSource.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class LocalFontFaceSource final : public CSSFontFaceSource {
 public:
  LocalFontFaceSource(const String& font_name) : font_name_(font_name) {}
  bool IsLocal() const override { return true; }
  bool IsLocalFontAvailable(const FontDescription&) override;

 private:
  RefPtr<SimpleFontData> CreateFontData(
      const FontDescription&,
      const FontSelectionCapabilities&) override;

  class LocalFontHistograms {
    DISALLOW_NEW();

   public:
    LocalFontHistograms() : reported_(false) {}
    void Record(bool load_success);

   private:
    bool reported_;
  };

  AtomicString font_name_;
  LocalFontHistograms histograms_;
};

}  // namespace blink

#endif
