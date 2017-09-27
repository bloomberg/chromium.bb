/*
 * Copyright (C) 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ThreadSafeRefCounted_h
#define ThreadSafeRefCounted_h

#include "base/atomic_ref_count.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/WTFExport.h"

namespace WTF {

class WTF_EXPORT ThreadSafeRefCountedBase {
  WTF_MAKE_NONCOPYABLE(ThreadSafeRefCountedBase);
  USING_FAST_MALLOC(ThreadSafeRefCountedBase);

 public:
  ThreadSafeRefCountedBase() : ref_count_(1) {}

  void Ref() { ref_count_.Increment(); }
  bool HasOneRef() const { return ref_count_.IsOne(); }

 protected:
  // Returns whether the pointer should be freed or not.
  bool DerefBase() { return !ref_count_.Decrement(); }

 private:
  base::AtomicRefCount ref_count_;
};

template <typename T, typename Traits>
class ThreadSafeRefCounted;

template <typename T>
struct DefaultThreadSafeRefCountedTraits {
  static void Destruct(const T* x) {
    WTF::ThreadSafeRefCounted<
        T, DefaultThreadSafeRefCountedTraits>::DeleteInternal(x);
  }
};

template <typename T, typename Traits = DefaultThreadSafeRefCountedTraits<T>>
class ThreadSafeRefCounted : public ThreadSafeRefCountedBase {
 public:
  void Deref() {
    if (DerefBase())
      Traits::Destruct(static_cast<T*>(this));
  }

 protected:
  ThreadSafeRefCounted() {}

 private:
  friend struct DefaultThreadSafeRefCountedTraits<T>;

  template <typename U>
  static void DeleteInternal(const U* x) {
    delete x;
  }
};

}  // namespace WTF

using WTF::ThreadSafeRefCounted;

#endif  // ThreadSafeRefCounted_h
