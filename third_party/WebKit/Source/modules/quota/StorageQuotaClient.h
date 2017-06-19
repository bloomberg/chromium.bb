/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef StorageQuotaClient_h
#define StorageQuotaClient_h

#include "core/page/Page.h"
#include "modules/ModulesExport.h"
#include "modules/quota/StorageQuotaClient.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "public/platform/WebStorageQuotaType.h"

namespace blink {

class ExecutionContext;
class Page;
class StorageErrorCallback;
class StorageQuotaCallback;

class MODULES_EXPORT StorageQuotaClient
    : public GarbageCollectedFinalized<StorageQuotaClient>,
      public Supplement<Page> {
  WTF_MAKE_NONCOPYABLE(StorageQuotaClient);
  USING_GARBAGE_COLLECTED_MIXIN(StorageQuotaClient);

 public:
  static StorageQuotaClient* Create() { return new StorageQuotaClient(); }

  virtual ~StorageQuotaClient();

  void RequestQuota(ScriptState*,
                    WebStorageQuotaType,
                    unsigned long long new_quota_in_bytes,
                    StorageQuotaCallback*,
                    StorageErrorCallback*);

  static const char* SupplementName();
  static StorageQuotaClient* From(ExecutionContext*);

  DEFINE_INLINE_VIRTUAL_TRACE() { Supplement<Page>::Trace(visitor); }

 private:
  StorageQuotaClient();
};

MODULES_EXPORT void ProvideStorageQuotaClientTo(Page&, StorageQuotaClient*);

}  // namespace blink

#endif  // StorageQuotaClient_h
