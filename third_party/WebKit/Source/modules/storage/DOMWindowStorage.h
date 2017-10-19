// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMWindowStorage_h
#define DOMWindowStorage_h

#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class LocalDOMWindow;
class Storage;

class DOMWindowStorage final : public GarbageCollected<DOMWindowStorage>,
                               public Supplement<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(DOMWindowStorage);

 public:
  static DOMWindowStorage& From(LocalDOMWindow&);
  static Storage* sessionStorage(LocalDOMWindow&, ExceptionState&);
  static Storage* localStorage(LocalDOMWindow&, ExceptionState&);

  Storage* sessionStorage(ExceptionState&) const;
  Storage* localStorage(ExceptionState&) const;
  Storage* OptionalSessionStorage() const { return session_storage_.Get(); }
  Storage* OptionalLocalStorage() const { return local_storage_.Get(); }

  void Trace(blink::Visitor*);

 private:
  explicit DOMWindowStorage(LocalDOMWindow&);
  static const char* SupplementName();

  mutable Member<Storage> session_storage_;
  mutable Member<Storage> local_storage_;
};

}  // namespace blink

#endif  // DOMWindowStorage_h
