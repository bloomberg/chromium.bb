/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef DeprecatedStorageQuota_h
#define DeprecatedStorageQuota_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ScriptState;
class StorageErrorCallback;
class StorageQuotaCallback;
class StorageUsageCallback;

class DeprecatedStorageQuota final
    : public GarbageCollected<DeprecatedStorageQuota>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Type {
    kTemporary,
    kPersistent,
  };

  static DeprecatedStorageQuota* Create(Type type) {
    return new DeprecatedStorageQuota(type);
  }

  void queryUsageAndQuota(ScriptState*,
                          StorageUsageCallback*,
                          StorageErrorCallback*);

  void requestQuota(ScriptState*,
                    unsigned long long new_quota_in_bytes,
                    StorageQuotaCallback*,
                    StorageErrorCallback*);

  DEFINE_INLINE_TRACE() {}

 private:
  explicit DeprecatedStorageQuota(Type);
  Type type_;
};

}  // namespace blink

#endif  // DeprecatedStorageQuota_h
