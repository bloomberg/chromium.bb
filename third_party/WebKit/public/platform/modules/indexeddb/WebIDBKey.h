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

#ifndef WebIDBKey_h
#define WebIDBKey_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebData.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"

namespace blink {

class IDBKey;

class WebIDBKey {
 public:
  // Please use one of the factory methods. This is public only to allow
  // WebVector.
  WebIDBKey() {}
  ~WebIDBKey() { Reset(); }

  BLINK_EXPORT static WebIDBKey CreateArray(const WebVector<WebIDBKey>&);
  BLINK_EXPORT static WebIDBKey CreateBinary(const WebData&);
  BLINK_EXPORT static WebIDBKey CreateString(const WebString&);
  BLINK_EXPORT static WebIDBKey CreateDate(double);
  BLINK_EXPORT static WebIDBKey CreateNumber(double);
  BLINK_EXPORT static WebIDBKey CreateInvalid();
  BLINK_EXPORT static WebIDBKey CreateNull();

  WebIDBKey(const WebIDBKey& e) { Assign(e); }
  WebIDBKey& operator=(const WebIDBKey& e) {
    Assign(e);
    return *this;
  }

  BLINK_EXPORT void Assign(const WebIDBKey&);
  BLINK_EXPORT void AssignArray(const WebVector<WebIDBKey>&);
  BLINK_EXPORT void AssignBinary(const WebData&);
  BLINK_EXPORT void AssignString(const WebString&);
  BLINK_EXPORT void AssignDate(double);
  BLINK_EXPORT void AssignNumber(double);
  BLINK_EXPORT void AssignInvalid();
  BLINK_EXPORT void AssignNull();
  BLINK_EXPORT void Reset();

  BLINK_EXPORT WebIDBKeyType KeyType() const;
  BLINK_EXPORT bool IsValid() const;
  BLINK_EXPORT WebVector<WebIDBKey> Array() const;  // Only valid for ArrayType.
  BLINK_EXPORT WebData Binary() const;       // Only valid for BinaryType.
  BLINK_EXPORT WebString GetString() const;  // Only valid for StringType.
  BLINK_EXPORT double Date() const;          // Only valid for DateType.
  BLINK_EXPORT double Number() const;        // Only valid for NumberType.

#if BLINK_IMPLEMENTATION
  WebIDBKey(IDBKey* value) : private_(value) {}
  WebIDBKey& operator=(IDBKey* value) {
    private_ = value;
    return *this;
  }
  operator IDBKey*() const { return private_.Get(); }
#endif

 private:
  WebPrivatePtr<IDBKey> private_;
};

}  // namespace blink

#endif  // WebIDBKey_h
