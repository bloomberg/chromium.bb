/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef FontData_h
#define FontData_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/Unicode.h"

namespace blink {

class SimpleFontData;

class PLATFORM_EXPORT FontData : public RefCounted<FontData> {
  WTF_MAKE_NONCOPYABLE(FontData);

 public:
  FontData() {}

  virtual ~FontData();

  virtual const SimpleFontData* FontDataForCharacter(UChar32) const = 0;
  virtual bool IsCustomFont() const = 0;
  virtual bool IsLoading() const = 0;
  // Returns whether this is a temporary font data for a custom font which is
  // not yet loaded.
  virtual bool IsLoadingFallback() const = 0;
  virtual bool IsSegmented() const = 0;
  virtual bool ShouldSkipDrawing() const = 0;
};

#define DEFINE_FONT_DATA_TYPE_CASTS(thisType, predicate)     \
  template <typename T>                                      \
  inline thisType* To##thisType(const scoped_refptr<T>& fontData) { \
    return To##thisType(fontData.get());                     \
  }                                                          \
  DEFINE_TYPE_CASTS(thisType, FontData, fontData,            \
                    fontData->IsSegmented() == predicate,    \
                    fontData.IsSegmented() == predicate)

}  // namespace blink

#endif  // FontData_h
