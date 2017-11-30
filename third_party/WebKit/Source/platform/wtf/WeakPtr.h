/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WTF_WeakPtr_h
#define WTF_WeakPtr_h

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "platform/wtf/Allocator.h"

namespace WTF {

template <typename T>
using WeakPtr = base::WeakPtr<T>;

template <typename T>
class WeakPtrFactory {
  USING_FAST_MALLOC(WeakPtrFactory);

 public:
  explicit WeakPtrFactory(T* ptr) : factory_(ptr) {}

  WeakPtr<T> CreateWeakPtr() { return factory_.GetWeakPtr(); }

  void RevokeAll() { factory_.InvalidateWeakPtrs(); }

  bool HasWeakPtrs() const { return factory_.HasWeakPtrs(); }

 private:
  base::WeakPtrFactory<T> factory_;

  DISALLOW_COPY_AND_ASSIGN(WeakPtrFactory<T>);
};

}  // namespace WTF

using WTF::WeakPtr;
using WTF::WeakPtrFactory;

#endif
