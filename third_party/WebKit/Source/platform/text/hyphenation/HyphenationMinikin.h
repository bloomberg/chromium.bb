// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

class PLATFORM_EXPORT HyphenationMinikin : public Hyphenation {
 public:
  bool openDictionary(const AtomicString& locale);

  size_t lastHyphenLocation(const StringView& text,
                            size_t beforeIndex) const override;
  Vector<size_t, 8> hyphenLocations(const StringView&) const override;

  static PassRefPtr<HyphenationMinikin> fromFileForTesting(base::File);

 private:
  bool openDictionary(base::File);

  std::vector<uint8_t> hyphenate(const StringView&) const;

  base::MemoryMappedFile m_file;
  std::unique_ptr<android::Hyphenator> m_hyphenator;
};

}  // namespace blink
