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

#ifndef CSSImageGeneratorValue_h
#define CSSImageGeneratorValue_h

#include "core/CoreExport.h"
#include "core/css/CSSValue.h"
#include "platform/geometry/IntSizeHash.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/wtf/HashCountedSet.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class Document;
class Image;
class FloatSize;
class ComputedStyle;
class ImageResourceObserver;

struct SizeAndCount {
  DISALLOW_NEW();
  SizeAndCount(IntSize new_size = IntSize(), int new_count = 0)
      : size(new_size), count(new_count) {}

  IntSize size;
  int count;
};

using ClientSizeCountMap = HashMap<const ImageResourceObserver*, SizeAndCount>;

class CORE_EXPORT CSSImageGeneratorValue : public CSSValue {
 public:
  ~CSSImageGeneratorValue();

  void AddClient(const ImageResourceObserver*, const IntSize&);
  void RemoveClient(const ImageResourceObserver*);
  // The |container_size| is the container size with subpixel snapping, where
  // the |logical_size| is without it. Both sizes include zoom.
  RefPtr<Image> GetImage(const ImageResourceObserver&,
                         const Document&,
                         const ComputedStyle&,
                         const IntSize& container_size,
                         const LayoutSize* logical_size);

  bool IsFixedSize() const;
  IntSize FixedSize(const Document&, const FloatSize& default_object_size);

  bool IsPending() const;
  bool KnownToBeOpaque(const Document&, const ComputedStyle&) const;

  void LoadSubimages(const Document&);

  CSSImageGeneratorValue* ValueWithURLsMadeAbsolute();

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    CSSValue::TraceAfterDispatch(visitor);
  }

 protected:
  explicit CSSImageGeneratorValue(ClassType);

  Image* GetImage(const ImageResourceObserver*,
                  const Document&,
                  const ComputedStyle&,
                  const IntSize&);
  void PutImage(const IntSize&, RefPtr<Image>);
  const ClientSizeCountMap& Clients() const { return clients_; }

  HashCountedSet<IntSize>
      sizes_;  // A count of how many times a given image size is in use.
  ClientSizeCountMap
      clients_;  // A map from LayoutObjects (with entry count) to image sizes.
  HashMap<IntSize, RefPtr<Image>>
      images_;  // A cache of Image objects by image size.

  // TODO(Oilpan): when/if we can make the layoutObject point directly to the
  // CSSImageGenerator value using a member we don't need to have this hack
  // where we keep a persistent to the instance as long as there are clients in
  // the ClientSizeCountMap.
  SelfKeepAlive<CSSImageGeneratorValue> keep_alive_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSImageGeneratorValue, IsImageGeneratorValue());

}  // namespace blink

#endif  // CSSImageGeneratorValue_h
