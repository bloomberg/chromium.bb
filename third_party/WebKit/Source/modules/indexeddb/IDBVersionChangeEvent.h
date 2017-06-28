/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef IDBVersionChangeEvent_h
#define IDBVersionChangeEvent_h

#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "modules/EventModules.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IDBVersionChangeEventInit.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"

namespace blink {

class IDBVersionChangeEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBVersionChangeEvent* Create() { return new IDBVersionChangeEvent(); }
  static IDBVersionChangeEvent* Create(
      const AtomicString& event_type,
      unsigned long long old_version,
      const Nullable<unsigned long long>& new_version,
      WebIDBDataLoss data_loss = kWebIDBDataLossNone,
      const String& data_loss_message = String()) {
    return new IDBVersionChangeEvent(event_type, old_version, new_version,
                                     data_loss, data_loss_message);
  }
  static IDBVersionChangeEvent* Create(
      const AtomicString& event_type,
      const IDBVersionChangeEventInit& initializer) {
    return new IDBVersionChangeEvent(event_type, initializer);
  }

  unsigned long long oldVersion() const { return old_version_; }
  unsigned long long newVersion(bool& is_null) const;

  const AtomicString& dataLoss() const;
  const String& dataLossMessage() const { return data_loss_message_; }

  const AtomicString& InterfaceName() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  IDBVersionChangeEvent();
  IDBVersionChangeEvent(const AtomicString& event_type,
                        unsigned long long old_version,
                        const Nullable<unsigned long long>& new_version,
                        WebIDBDataLoss,
                        const String& data_loss);
  IDBVersionChangeEvent(const AtomicString& event_type,
                        const IDBVersionChangeEventInit&);

  unsigned long long old_version_;
  Nullable<unsigned long long> new_version_;
  WebIDBDataLoss data_loss_;
  String data_loss_message_;
};

}  // namespace blink

#endif  // IDBVersionChangeEvent_h
