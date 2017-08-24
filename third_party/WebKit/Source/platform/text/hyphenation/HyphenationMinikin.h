// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HyphenationMinikin_h
#define HyphenationMinikin_h

#include "platform/text/Hyphenation.h"

#include "base/files/memory_mapped_file.h"
#include "platform/PlatformExport.h"

namespace base {
class File;
}  // namespace base

namespace android {
class Hyphenator;
}  // namespace andorid

namespace blink {

class PLATFORM_EXPORT HyphenationMinikin final : public Hyphenation {
 public:
  bool OpenDictionary(const AtomicString& locale);

  size_t LastHyphenLocation(const StringView& text,
                            size_t before_index) const override;
  Vector<size_t, 8> HyphenLocations(const StringView&) const override;

  static RefPtr<HyphenationMinikin> FromFileForTesting(base::File);

 private:
  bool OpenDictionary(base::File);

  std::vector<uint8_t> Hyphenate(const StringView&) const;

  base::MemoryMappedFile file_;
  std::unique_ptr<android::Hyphenator> hyphenator_;
};

}  // namespace blink

#endif  // HyphenationMinikin_h
