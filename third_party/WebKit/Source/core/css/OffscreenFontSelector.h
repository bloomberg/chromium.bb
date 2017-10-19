// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenFontSelector_h
#define OffscreenFontSelector_h

#include "core/CoreExport.h"
#include "core/css/FontFaceCache.h"
#include "platform/fonts/FontSelector.h"
#include "platform/fonts/GenericFontFamilySettings.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"

namespace blink {

class ExecutionContext;
class FontDescription;

class CORE_EXPORT OffscreenFontSelector : public FontSelector {
 public:
  static OffscreenFontSelector* Create(ExecutionContext* context) {
    return new OffscreenFontSelector(context);
  }
  ~OffscreenFontSelector() override;

  unsigned Version() const override { return 1; }

  void ReportNotDefGlyph() const override;

  RefPtr<FontData> GetFontData(const FontDescription&,
                               const AtomicString&) override;
  void WillUseFontData(const FontDescription&,
                       const AtomicString& family,
                       const String& text) override;
  void WillUseRange(const FontDescription&,
                    const AtomicString& family_name,
                    const FontDataForRangeSet&) override;

  void RegisterForInvalidationCallbacks(FontSelectorClient*) override;
  void UnregisterForInvalidationCallbacks(FontSelectorClient*) override;

  const GenericFontFamilySettings& GetGenericFontFamilySettings() const {
    return generic_font_family_settings_;
  }

  void FontCacheInvalidated() override;
  void FontFaceInvalidated() override;

  void UpdateGenericFontFamilySettings(const GenericFontFamilySettings&);

  FontFaceCache* GetFontFaceCache() { return &font_face_cache_; }

  bool IsPlatformFamilyMatchAvailable(const FontDescription&,
                                      const AtomicString& passed_family);

  ExecutionContext* GetExecutionContext() const override {
    return execution_context_;
  }

  virtual void Trace(blink::Visitor*);

 protected:
  explicit OffscreenFontSelector(ExecutionContext*);

  void DispatchInvalidationCallbacks();

 private:
  GenericFontFamilySettings generic_font_family_settings_;

  FontFaceCache font_face_cache_;

  Member<ExecutionContext> execution_context_;
};

}  // namespace blink

#endif  // OffscreenFontSelector_h
