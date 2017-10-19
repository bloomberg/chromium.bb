// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasFontCache_h
#define CanvasFontCache_h

#include <memory>
#include "core/CoreExport.h"
#include "core/css/StylePropertySet.h"
#include "platform/fonts/Font.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/ListHashSet.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebThread.h"

namespace blink {

class ComputedStyle;
class Document;
class FontCachePurgePreventer;

class CORE_EXPORT CanvasFontCache final
    : public GarbageCollectedFinalized<CanvasFontCache>,
      public WebThread::TaskObserver {
 public:
  static CanvasFontCache* Create(Document& document) {
    return new CanvasFontCache(document);
  }

  MutableStylePropertySet* ParseFont(const String&);
  void PruneAll();
  unsigned size();

  virtual void Trace(blink::Visitor*);

  static unsigned MaxFonts();
  unsigned HardMaxFonts();

  void WillUseCurrentFont() { SchedulePruningIfNeeded(); }
  bool GetFontUsingDefaultStyle(const String&, Font&);

  // TaskObserver implementation
  void DidProcessTask() override;
  void WillProcessTask() override {}

  // For testing
  bool IsInCache(const String&);

  ~CanvasFontCache();

 private:
  explicit CanvasFontCache(Document&);
  void SchedulePruningIfNeeded();
  typedef HeapHashMap<String, Member<MutableStylePropertySet>>
      MutableStylePropertyMap;

  HashMap<String, Font> fonts_resolved_using_default_style_;
  MutableStylePropertyMap fetched_fonts_;
  ListHashSet<String> font_lru_list_;
  std::unique_ptr<FontCachePurgePreventer> main_cache_purge_preventer_;
  Member<Document> document_;
  scoped_refptr<ComputedStyle> default_font_style_;
  bool pruning_scheduled_;
};

}  // namespace blink

#endif
