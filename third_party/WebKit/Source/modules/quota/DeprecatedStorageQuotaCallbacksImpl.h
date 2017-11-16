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
#include "modules/ModulesExport.h"
#include "modules/quota/StorageErrorCallback.h"
#include "modules/quota/StorageQuotaCallback.h"
#include "modules/quota/StorageUsageCallback.h"
#include "platform/StorageQuotaCallbacks.h"

namespace blink {

class MODULES_EXPORT DeprecatedStorageQuotaCallbacksImpl final
    : public StorageQuotaCallbacks {
 public:
  static DeprecatedStorageQuotaCallbacksImpl* Create(
      StorageUsageCallback* success,
      StorageErrorCallback* error) {
    return new DeprecatedStorageQuotaCallbacksImpl(success, error);
  }

  static DeprecatedStorageQuotaCallbacksImpl* Create(
      StorageQuotaCallback* success,
      StorageErrorCallback* error) {
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
  DeprecatedStorageQuotaCallbacksImpl(StorageUsageCallback*,
                                      StorageErrorCallback*);
  DeprecatedStorageQuotaCallbacksImpl(StorageQuotaCallback*,
                                      StorageErrorCallback*);

  Member<StorageUsageCallback> usage_callback_;
  Member<StorageQuotaCallback> quota_callback_;
  Member<StorageErrorCallback> error_callback_;
};

}  // namespace blink

#endif  // DeprecatedStorageQuotaCallbacksImpl_h
