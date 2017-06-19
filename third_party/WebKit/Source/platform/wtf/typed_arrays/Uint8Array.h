/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef Uint8Array_h
#define Uint8Array_h

#include "platform/wtf/typed_arrays/IntegralTypedArrayBase.h"

namespace WTF {

class ArrayBuffer;

class Uint8Array : public IntegralTypedArrayBase<unsigned char> {
 public:
  static inline RefPtr<Uint8Array> Create(unsigned length);
  static inline RefPtr<Uint8Array> Create(const unsigned char* array,
                                          unsigned length);
  static inline RefPtr<Uint8Array> Create(RefPtr<ArrayBuffer>,
                                          unsigned byte_offset,
                                          unsigned length);

  using TypedArrayBase<unsigned char>::Set;
  using IntegralTypedArrayBase<unsigned char>::Set;

  ViewType GetType() const override { return kTypeUint8; }

 protected:
  inline Uint8Array(RefPtr<ArrayBuffer>, unsigned byte_offset, unsigned length);
  // Make constructor visible to superclass.
  friend class TypedArrayBase<unsigned char>;
};

RefPtr<Uint8Array> Uint8Array::Create(unsigned length) {
  return TypedArrayBase<unsigned char>::Create<Uint8Array>(length);
}

RefPtr<Uint8Array> Uint8Array::Create(const unsigned char* array,
                                      unsigned length) {
  return TypedArrayBase<unsigned char>::Create<Uint8Array>(array, length);
}

RefPtr<Uint8Array> Uint8Array::Create(RefPtr<ArrayBuffer> buffer,
                                      unsigned byte_offset,
                                      unsigned length) {
  return TypedArrayBase<unsigned char>::Create<Uint8Array>(std::move(buffer),
                                                           byte_offset, length);
}

Uint8Array::Uint8Array(RefPtr<ArrayBuffer> buffer,
                       unsigned byte_offset,
                       unsigned length)
    : IntegralTypedArrayBase<unsigned char>(std::move(buffer),
                                            byte_offset,
                                            length) {}

}  // namespace WTF

using WTF::Uint8Array;

#endif  // Uint8Array_h
