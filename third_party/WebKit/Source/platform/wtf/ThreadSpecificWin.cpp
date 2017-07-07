/*
 * Copyright (C) 2009 Jian Li <jianli@chromium.org>
 * Copyright (C) 2012 Patrick Gansterer <paroga@paroga.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "platform/wtf/ThreadSpecific.h"

#include "build/build_config.h"

#if defined(OS_WIN)

#include "platform/wtf/Allocator.h"
#include "platform/wtf/DoublyLinkedList.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/ThreadingPrimitives.h"

namespace WTF {

static DoublyLinkedList<PlatformThreadSpecificKey>& DestructorsList() {
  DEFINE_STATIC_LOCAL(DoublyLinkedList<PlatformThreadSpecificKey>, static_list,
                      ());
  return static_list;
}

static Mutex& DestructorsMutex() {
  DEFINE_STATIC_LOCAL(Mutex, static_mutex, ());
  return static_mutex;
}

class PlatformThreadSpecificKey
    : public DoublyLinkedListNode<PlatformThreadSpecificKey> {
  USING_FAST_MALLOC(PlatformThreadSpecificKey);
  WTF_MAKE_NONCOPYABLE(PlatformThreadSpecificKey);

 public:
  friend class DoublyLinkedListNode<PlatformThreadSpecificKey>;

  PlatformThreadSpecificKey(void (*destructor)(void*))
      : destructor_(destructor) {
    tls_key_ = TlsAlloc();
    CHECK_NE(tls_key_, TLS_OUT_OF_INDEXES);
  }

  ~PlatformThreadSpecificKey() { TlsFree(tls_key_); }

  void SetValue(void* data) { TlsSetValue(tls_key_, data); }
  void* Value() { return TlsGetValue(tls_key_); }

  void CallDestructor() {
    if (void* data = Value())
      destructor_(data);
  }

 private:
  void (*destructor_)(void*);
  DWORD tls_key_;
  PlatformThreadSpecificKey* prev_;
  PlatformThreadSpecificKey* next_;
};

long& TlsKeyCount() {
  static long count;
  return count;
}

DWORD* TlsKeys() {
  static DWORD keys[kMaxTlsKeySize];
  return keys;
}

void ThreadSpecificKeyCreate(ThreadSpecificKey* key,
                             void (*destructor)(void*)) {
  *key = new PlatformThreadSpecificKey(destructor);

  MutexLocker locker(DestructorsMutex());
  DestructorsList().Push(*key);
}

void ThreadSpecificKeyDelete(ThreadSpecificKey key) {
  MutexLocker locker(DestructorsMutex());
  DestructorsList().Remove(key);
  delete key;
}

void ThreadSpecificSet(ThreadSpecificKey key, void* data) {
  key->SetValue(data);
}

void* ThreadSpecificGet(ThreadSpecificKey key) {
  return key->Value();
}

void ThreadSpecificThreadExit() {
  for (long i = 0; i < TlsKeyCount(); i++) {
    // The layout of ThreadSpecific<T>::Data does not depend on T. So we are
    // safe to do the static cast to ThreadSpecific<int> in order to access its
    // data member.
    ThreadSpecific<int>::Data* data =
        static_cast<ThreadSpecific<int>::Data*>(TlsGetValue(TlsKeys()[i]));
    if (data)
      data->destructor(data);
  }

  MutexLocker locker(DestructorsMutex());
  PlatformThreadSpecificKey* key = DestructorsList().Head();
  while (key) {
    PlatformThreadSpecificKey* next_key = key->Next();
    key->CallDestructor();
    key = next_key;
  }
}

}  // namespace WTF

#endif  // defined(OS_WIN)
