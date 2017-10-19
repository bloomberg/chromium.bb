/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef Storage_h
#define Storage_h

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "modules/storage/StorageArea.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class ExceptionState;
class LocalFrame;

class Storage final : public GarbageCollected<Storage>,
                      public ScriptWrappable,
                      public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Storage);

 public:
  static Storage* Create(LocalFrame*, StorageArea*);
  unsigned length(ExceptionState& ec) const {
    return storage_area_->length(ec, GetFrame());
  }
  String key(unsigned index, ExceptionState& ec) const {
    return storage_area_->Key(index, ec, GetFrame());
  }
  String getItem(const String& key, ExceptionState& ec) const {
    return storage_area_->GetItem(key, ec, GetFrame());
  }
  bool setItem(const String& key, const String& value, ExceptionState& ec) {
    storage_area_->SetItem(key, value, ec, GetFrame());
    return true;
  }
  DeleteResult removeItem(const String& key, ExceptionState& ec) {
    storage_area_->RemoveItem(key, ec, GetFrame());
    return kDeleteSuccess;
  }
  void clear(ExceptionState& ec) { storage_area_->Clear(ec, GetFrame()); }
  bool Contains(const String& key, ExceptionState& ec) const {
    return storage_area_->Contains(key, ec, GetFrame());
  }

  StorageArea* Area() const { return storage_area_.Get(); }

  void NamedPropertyEnumerator(Vector<String>&, ExceptionState&);
  bool NamedPropertyQuery(const AtomicString&, ExceptionState&);

  virtual void Trace(blink::Visitor*);

 private:
  Storage(LocalFrame*, StorageArea*);

  Member<StorageArea> storage_area_;
};

}  // namespace blink

#endif  // Storage_h
