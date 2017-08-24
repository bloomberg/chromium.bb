// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Hyphenation_h
#define Hyphenation_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/text/AtomicStringHash.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

class Font;

class PLATFORM_EXPORT Hyphenation : public RefCounted<Hyphenation> {
 public:
  virtual ~Hyphenation() {}

  // Find the last hyphenation location before |before_index|.
  // Returns 0 if no hyphenation locations were found.
  virtual size_t LastHyphenLocation(const StringView&,
                                    size_t before_index) const = 0;

  // Find the first hyphenation location after |after_index|.
  // Returns 0 if no hyphenation locations were found.
  virtual size_t FirstHyphenLocation(const StringView&,
                                     size_t after_index) const;

  // Find all hyphenation locations in the reverse order.
  virtual Vector<size_t, 8> HyphenLocations(const StringView&) const;

  static const unsigned kMinimumPrefixLength = 2;
  static const unsigned kMinimumSuffixLength = 2;
  static int MinimumPrefixWidth(const Font&);

 private:
  friend class LayoutLocale;
  static RefPtr<Hyphenation> PlatformGetHyphenation(const AtomicString& locale);
};

}  // namespace blink

#endif  // Hyphenation_h
