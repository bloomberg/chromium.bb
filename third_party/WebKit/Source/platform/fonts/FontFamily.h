/*
 * Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FontFamily_h
#define FontFamily_h

#include "platform/PlatformExport.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class SharedFontFamily;

class PLATFORM_EXPORT FontFamily {
  DISALLOW_NEW();

 public:
  FontFamily() {}
  ~FontFamily();

  void SetFamily(const AtomicString& family) { family_ = family; }
  const AtomicString& Family() const { return family_; }
  bool FamilyIsEmpty() const { return family_.IsEmpty(); }

  const FontFamily* Next() const;

  void AppendFamily(scoped_refptr<SharedFontFamily>);
  scoped_refptr<SharedFontFamily> ReleaseNext();

  // Returns this font family's name followed by all subsequent linked
  // families delimited by commas.
  String ToString() const;

 private:
  AtomicString family_;
  scoped_refptr<SharedFontFamily> next_;
};

class PLATFORM_EXPORT SharedFontFamily : public FontFamily,
                                         public RefCounted<SharedFontFamily> {
  USING_FAST_MALLOC(SharedFontFamily);
  WTF_MAKE_NONCOPYABLE(SharedFontFamily);

 public:
  static scoped_refptr<SharedFontFamily> Create() {
    return WTF::AdoptRef(new SharedFontFamily);
  }

 private:
  SharedFontFamily() {}
};

PLATFORM_EXPORT bool operator==(const FontFamily&, const FontFamily&);
inline bool operator!=(const FontFamily& a, const FontFamily& b) {
  return !(a == b);
}

inline FontFamily::~FontFamily() {
  scoped_refptr<SharedFontFamily> reaper = std::move(next_);
  while (reaper && reaper->HasOneRef()) {
    // implicitly protects reaper->next, then derefs reaper
    reaper = reaper->ReleaseNext();
  }
}

inline const FontFamily* FontFamily::Next() const {
  return next_.get();
}

inline void FontFamily::AppendFamily(scoped_refptr<SharedFontFamily> family) {
  next_ = std::move(family);
}

inline scoped_refptr<SharedFontFamily> FontFamily::ReleaseNext() {
  return std::move(next_);
}

}  // namespace blink

#endif
