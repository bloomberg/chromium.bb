// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_MODULES_CACHESTORAGE_INSPECTORCACHESTORAGEAGENT_H_
#define THIRD_PARTY_WEBKIT_SOURCE_MODULES_CACHESTORAGE_INSPECTORCACHESTORAGEAGENT_H_

#include <memory>

#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/CacheStorage.h"
#include "modules/ModulesExport.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class InspectedFrames;

class MODULES_EXPORT InspectorCacheStorageAgent final
    : public InspectorBaseAgent<protocol::CacheStorage::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorCacheStorageAgent);

 public:
  static InspectorCacheStorageAgent* Create(InspectedFrames* frames) {
    return new InspectorCacheStorageAgent(frames);
  }

  ~InspectorCacheStorageAgent() override;
  DECLARE_VIRTUAL_TRACE();

  void requestCacheNames(const String& security_origin,
                         std::unique_ptr<RequestCacheNamesCallback>) override;
  void requestEntries(const String& cache_id,
                      int skip_count,
                      int page_size,
                      std::unique_ptr<RequestEntriesCallback>) override;
  void deleteCache(const String& cache_id,
                   std::unique_ptr<DeleteCacheCallback>) override;
  void deleteEntry(const String& cache_id,
                   const String& request,
                   std::unique_ptr<DeleteEntryCallback>) override;
  void requestCachedResponse(
      const String& cache_id,
      const String& request_url,
      std::unique_ptr<RequestCachedResponseCallback>) override;

 private:
  explicit InspectorCacheStorageAgent(InspectedFrames*);

  Member<InspectedFrames> frames_;
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_MODULES_CACHESTORAGE_INSPECTORCACHESTORAGEAGENT_H_
