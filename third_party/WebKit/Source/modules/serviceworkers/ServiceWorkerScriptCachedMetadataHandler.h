// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerScriptCachedMetadataHandler_h
#define ServiceWorkerScriptCachedMetadataHandler_h

#include <stdint.h>
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/CachedMetadataHandler.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Vector.h"

namespace blink {

class WorkerGlobalScope;
class CachedMetadata;

class ServiceWorkerScriptCachedMetadataHandler : public CachedMetadataHandler {
 public:
  static ServiceWorkerScriptCachedMetadataHandler* Create(
      WorkerGlobalScope* worker_global_scope,
      const KURL& script_url,
      const Vector<char>* meta_data) {
    return new ServiceWorkerScriptCachedMetadataHandler(worker_global_scope,
                                                        script_url, meta_data);
  }
  ~ServiceWorkerScriptCachedMetadataHandler() override;
  virtual void Trace(blink::Visitor*);
  void SetCachedMetadata(uint32_t data_type_id,
                         const char*,
                         size_t,
                         CacheType) override;
  void ClearCachedMetadata(CacheType) override;
  scoped_refptr<CachedMetadata> GetCachedMetadata(
      uint32_t data_type_id) const override;
  String Encoding() const override;

 private:
  ServiceWorkerScriptCachedMetadataHandler(WorkerGlobalScope*,
                                           const KURL& script_url,
                                           const Vector<char>* meta_data);

  Member<WorkerGlobalScope> worker_global_scope_;
  KURL script_url_;
  scoped_refptr<CachedMetadata> cached_metadata_;
};

}  // namespace blink

#endif  // ServiceWorkerScriptCachedMetadataHandler_h
