/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebPrivatePtr_h
#define WebPrivatePtr_h

#include "WebCommon.h"
#include "base/logging.h"

#if INSIDE_BLINK
#include "platform/heap/Handle.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/TypeTraits.h"
#endif

namespace WTF {
template <class T>
class ThreadSafeRefCounted;
}

namespace blink {

// By default, the destruction of a WebPrivatePtr<> must happen on the same
// thread that created it, but can optionally be allowed to happen on
// another thread.
enum WebPrivatePtrDestruction {
  kWebPrivatePtrDestructionSameThread,
  kWebPrivatePtrDestructionCrossThread,
};

// The WebPrivatePtr<> holds by default a strong reference to its Blink object,
// but Blink GC managed objects also support keeping a weak reference by
// way of WebPrivatePtr<>.
enum class WebPrivatePtrStrength {
  kNormal,
  kWeak,
};

#if INSIDE_BLINK
enum LifetimeManagementType {
  kRefCountedLifetime,
  kGarbageCollectedLifetime,
};

template <typename T>
struct LifetimeOf {
 private:
  static const bool kIsGarbageCollected =
      WTF::IsSubclassOfTemplate<T, GarbageCollected>::value ||
      IsGarbageCollectedMixin<T>::value;

 public:
  static const LifetimeManagementType value =
      !kIsGarbageCollected ? kRefCountedLifetime : kGarbageCollectedLifetime;
};

template <typename T,
          WebPrivatePtrDestruction crossThreadDestruction,
          WebPrivatePtrStrength strongOrWeak,
          LifetimeManagementType lifetime>
class PtrStorageImpl;

template <typename T,
          WebPrivatePtrDestruction crossThreadDestruction,
          WebPrivatePtrStrength strongOrWeak>
class PtrStorageImpl<T,
                     crossThreadDestruction,
                     strongOrWeak,
                     kRefCountedLifetime> {
 public:
  typedef PassRefPtr<T> BlinkPtrType;

  void Assign(BlinkPtrType&& val) {
    static_assert(
        crossThreadDestruction == kWebPrivatePtrDestructionSameThread ||
            WTF::IsSubclassOfTemplate<T, WTF::ThreadSafeRefCounted>::value,
        "Cross thread destructible class must derive from "
        "ThreadSafeRefCounted<>");
    static_assert(
        strongOrWeak == WebPrivatePtrStrength::kNormal,
        "Ref-counted classes do not support weak WebPrivatePtr<> references");
    Release();
    ptr_ = val.LeakRef();
  }

  void Assign(const PtrStorageImpl& other) {
    T* val = other.Get();
    if (ptr_ == val)
      return;
    Release();
    WTF::RefIfNotNull(val);
    ptr_ = val;
  }

  T* Get() const { return ptr_; }

  void Release() {
    WTF::DerefIfNotNull(ptr_);
    ptr_ = 0;
  }

 private:
  T* ptr_;
};

template <typename T, WebPrivatePtrDestruction, WebPrivatePtrStrength>
struct WebPrivatePtrPersistentStorageType {
 public:
  using Type = Persistent<T>;
};

template <typename T>
struct WebPrivatePtrPersistentStorageType<T,
                                          kWebPrivatePtrDestructionSameThread,
                                          WebPrivatePtrStrength::kWeak> {
 public:
  using Type = WeakPersistent<T>;
};

template <typename T>
struct WebPrivatePtrPersistentStorageType<T,
                                          kWebPrivatePtrDestructionCrossThread,
                                          WebPrivatePtrStrength::kNormal> {
 public:
  using Type = CrossThreadPersistent<T>;
};

template <typename T>
struct WebPrivatePtrPersistentStorageType<T,
                                          kWebPrivatePtrDestructionCrossThread,
                                          WebPrivatePtrStrength::kWeak> {
 public:
  using Type = CrossThreadWeakPersistent<T>;
};

template <typename T,
          WebPrivatePtrDestruction crossThreadDestruction,
          WebPrivatePtrStrength strongOrWeak>
class PtrStorageImpl<T,
                     crossThreadDestruction,
                     strongOrWeak,
                     kGarbageCollectedLifetime> {
 public:
  using BlinkPtrType =
      typename WebPrivatePtrPersistentStorageType<T,
                                                  crossThreadDestruction,
                                                  strongOrWeak>::Type;

  void Assign(T* val) {
    if (!val) {
      Release();
      return;
    }

    if (!handle_)
      handle_ = new BlinkPtrType;

    (*handle_) = val;
  }

  template <typename U>
  void Assign(U* val) {
    Assign(static_cast<T*>(val));
  }

  void Assign(const PtrStorageImpl& other) { Assign(other.Get()); }

  T* Get() const { return handle_ ? handle_->Get() : 0; }

  void Release() {
    delete handle_;
    handle_ = 0;
  }

 private:
  BlinkPtrType* handle_;
};

template <typename T,
          WebPrivatePtrDestruction crossThreadDestruction,
          WebPrivatePtrStrength strongOrWeak>
class PtrStorage : public PtrStorageImpl<T,
                                         crossThreadDestruction,
                                         strongOrWeak,
                                         LifetimeOf<T>::value> {
 public:
  static PtrStorage& FromSlot(void** slot) {
    static_assert(sizeof(PtrStorage) == sizeof(void*),
                  "PtrStorage must be the size of a pointer");
    return *reinterpret_cast<PtrStorage*>(slot);
  }

  static const PtrStorage& FromSlot(void* const* slot) {
    static_assert(sizeof(PtrStorage) == sizeof(void*),
                  "PtrStorage must be the size of a pointer");
    return *reinterpret_cast<const PtrStorage*>(slot);
  }

 private:
  // Prevent construction via normal means.
  PtrStorage();
  PtrStorage(const PtrStorage&);
};
#endif

// This class is an implementation detail of the Blink API. It exists to help
// simplify the implementation of Blink interfaces that merely wrap a reference
// counted WebCore class.
//
// A typical implementation of a class which uses WebPrivatePtr might look like
// this:
//    class WebFoo {
//    public:
//        BLINK_EXPORT ~WebFoo();
//        WebFoo() { }
//        WebFoo(const WebFoo& other) { assign(other); }
//        WebFoo& operator=(const WebFoo& other)
//        {
//            assign(other);
//            return *this;
//        }
//        BLINK_EXPORT void assign(const WebFoo&);  // Implemented in the body.
//
//        // Methods that are exposed to Chromium and which are specific to
//        // WebFoo go here.
//        BLINK_EXPORT doWebFooThing();
//
//        // Methods that are used only by other Blink classes should only be
//        // declared when INSIDE_BLINK is set.
//    #if INSIDE_BLINK
//        WebFoo(WTF::PassRefPtr<Foo>);
//    #endif
//
//    private:
//        WebPrivatePtr<Foo> m_private;
//    };
//
//    // WebFoo.cpp
//    WebFoo::~WebFoo() { m_private.reset(); }
//    void WebFoo::assign(const WebFoo& other) { ... }
//
template <typename T,
          WebPrivatePtrDestruction crossThreadDestruction =
              kWebPrivatePtrDestructionSameThread,
          WebPrivatePtrStrength strongOrWeak = WebPrivatePtrStrength::kNormal>
class WebPrivatePtr {
 public:
  WebPrivatePtr() : storage_(0) {}
  ~WebPrivatePtr() {
    // We don't destruct the object pointed by m_ptr here because we don't
    // want to expose destructors of core classes to embedders. We should
    // call reset() manually in destructors of classes with WebPrivatePtr
    // members.
    DCHECK(!storage_);
  }

  bool IsNull() const { return !storage_; }
  explicit operator bool() const { return !IsNull(); }

#if INSIDE_BLINK
  template <typename U>
  WebPrivatePtr(U&& ptr) : storage_(0) {
    Storage().Assign(std::forward<U>(ptr));
  }

  void Reset() { Storage().Release(); }

  WebPrivatePtr& operator=(const WebPrivatePtr& other) {
    Storage().Assign(other.Storage());
    return *this;
  }

  template <typename U>
  WebPrivatePtr& operator=(U&& ptr) {
    Storage().Assign(std::forward<U>(ptr));
    return *this;
  }

  T* Get() const { return Storage().Get(); }

  T& operator*() const {
    DCHECK(storage_);
    return *Get();
  }

  T* operator->() const {
    DCHECK(storage_);
    return Get();
  }
#endif

 private:
#if INSIDE_BLINK
  using PtrStorageType = PtrStorage<T, crossThreadDestruction, strongOrWeak>;

  PtrStorageType& Storage() { return PtrStorageType::FromSlot(&storage_); }
  const PtrStorageType& Storage() const {
    return PtrStorageType::FromSlot(&storage_);
  }
#endif

#if !INSIDE_BLINK
  // Disable the assignment operator; we define it above for when
  // INSIDE_BLINK is set, but we need to make sure that it is not
  // used outside there; the compiler-provided version won't handle reference
  // counting properly.
  WebPrivatePtr& operator=(const WebPrivatePtr& other) = delete;
#endif
  // Disable the copy constructor; classes that contain a WebPrivatePtr
  // should implement their copy constructor using assign().
  WebPrivatePtr(const WebPrivatePtr&) = delete;

  void* storage_;
};

}  // namespace blink

#endif  // WebPrivatePtr_h
