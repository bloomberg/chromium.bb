// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontGlobalContext_h
#define FontGlobalContext_h

#include "platform/PlatformExport.h"
#include "platform/fonts/FontCache.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

enum CreateIfNeeded { kDoNotCreate, kCreate };

// FontGlobalContext contains non-thread-safe, thread-specific data used for
// font formatting.
class PLATFORM_EXPORT FontGlobalContext {
  WTF_MAKE_NONCOPYABLE(FontGlobalContext);

 public:
  static FontGlobalContext* Get(CreateIfNeeded = kCreate);

  static inline FontCache& GetFontCache() { return Get()->font_cache; }

  // Called by MemoryCoordinator to clear memory.
  static void ClearMemory();

 private:
  friend class WTF::ThreadSpecific<FontGlobalContext>;

  FontGlobalContext();

  FontCache font_cache;
};

}  // namespace blink

#endif
