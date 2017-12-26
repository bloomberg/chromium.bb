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

#ifndef DeprecatedStorageQuotaCallbacksImpl_h
#define DeprecatedStorageQuotaCallbacksImpl_h

#include "base/memory/scoped_refptr.h"
#include "bindings/modules/v8/v8_storage_error_callback.h"
#include "bindings/modules/v8/v8_storage_quota_callback.h"
#include "bindings/modules/v8/v8_storage_usage_callback.h"
#include "modules/ModulesExport.h"
#include "platform/StorageQuotaCallbacks.h"

namespace blink {

class MODULES_EXPORT DeprecatedStorageQuotaCallbacksImpl final
    : public StorageQuotaCallbacks {
 public:
  static DeprecatedStorageQuotaCallbacksImpl* Create(
      V8StorageUsageCallback* success,
      V8StorageErrorCallback* error) {
    return new DeprecatedStorageQuotaCallbacksImpl(success, error);
  }

  static DeprecatedStorageQuotaCallbacksImpl* Create(
      V8StorageQuotaCallback* success,
      V8StorageErrorCallback* error) {
    return new DeprecatedStorageQuotaCallbacksImpl(success, error);
  }

  ~DeprecatedStorageQuotaCallbacksImpl() override;
  virtual void Trace(blink::Visitor*);

  void DidQueryStorageUsageAndQuota(unsigned long long usage_in_bytes,
                                    unsigned long long quota_in_bytes) override;
  void DidGrantStorageQuota(unsigned long long usage_in_bytes,
                            unsigned long long granted_quota_in_bytes) override;
  void DidFail(WebStorageQuotaError) override;

 private:
  DeprecatedStorageQuotaCallbacksImpl(V8StorageUsageCallback*,
                                      V8StorageErrorCallback*);
  DeprecatedStorageQuotaCallbacksImpl(V8StorageQuotaCallback*,
                                      V8StorageErrorCallback*);

  // This set of callbacks is held by WebStorageQuotaCallbacks and passed in
  // Platform::QueryStorageUsageAndQuota, which doesn't support wrapper-tracing,
  // and there is no other owner of this object. Thus, this object holds the
  // underlying callback functions as persistent handles. This is acceptable
  // because this object will be discarded in a limited time once
  // Platform::QueryStorageUsageAndQuota finishes a task.
  V8StorageUsageCallback::Persistent<V8StorageUsageCallback> usage_callback_;
  V8StorageQuotaCallback::Persistent<V8StorageQuotaCallback> quota_callback_;
  V8StorageErrorCallback::Persistent<V8StorageErrorCallback> error_callback_;
};

}  // namespace blink

#endif  // DeprecatedStorageQuotaCallbacksImpl_h
