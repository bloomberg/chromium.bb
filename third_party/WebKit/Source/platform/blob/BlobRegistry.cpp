/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "platform/blob/BlobRegistry.h"

#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/blob/BlobData.h"
#include "platform/blob/BlobURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/URLSecurityOriginMap.h"
#include "public/platform/Platform.h"
#include "public/platform/WebBlobData.h"
#include "public/platform/WebBlobRegistry.h"
#include "public/platform/WebString.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Assertions.h"
#include "wtf/HashMap.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class BlobOriginMap : public URLSecurityOriginMap {
 public:
  BlobOriginMap();
  SecurityOrigin* GetOrigin(const KURL&) override;
};

static WebBlobRegistry* GetBlobRegistry() {
  return Platform::Current()->GetBlobRegistry();
}

typedef HashMap<String, RefPtr<SecurityOrigin>> BlobURLOriginMap;
static ThreadSpecific<BlobURLOriginMap>& OriginMap() {
  // We want to create the BlobOriginMap exactly once because it is shared by
  // all the threads.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(BlobOriginMap, cache, new BlobOriginMap);
  (void)cache;  // BlobOriginMap's constructor does the interesting work.

  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<BlobURLOriginMap>, map,
                                  new ThreadSpecific<BlobURLOriginMap>);
  return map;
}

static void SaveToOriginMap(SecurityOrigin* origin, const KURL& url) {
  // If the blob URL contains null origin, as in the context with unique
  // security origin or file URL, save the mapping between url and origin so
  // that the origin can be retrieved when doing security origin check.
  //
  // See the definition of the origin of a Blob URL in the File API spec.
  if (origin && BlobURL::GetOrigin(url) == "null")
    OriginMap()->insert(url.GetString(), origin);
}

static void RemoveFromOriginMap(const KURL& url) {
  if (BlobURL::GetOrigin(url) == "null")
    OriginMap()->erase(url.GetString());
}

void BlobRegistry::RegisterBlobData(const String& uuid,
                                    std::unique_ptr<BlobData> data) {
  GetBlobRegistry()->RegisterBlobData(uuid, WebBlobData(std::move(data)));
}

void BlobRegistry::AddBlobDataRef(const String& uuid) {
  GetBlobRegistry()->AddBlobDataRef(uuid);
}

void BlobRegistry::RemoveBlobDataRef(const String& uuid) {
  GetBlobRegistry()->RemoveBlobDataRef(uuid);
}

void BlobRegistry::RegisterPublicBlobURL(SecurityOrigin* origin,
                                         const KURL& url,
                                         PassRefPtr<BlobDataHandle> handle) {
  SaveToOriginMap(origin, url);
  GetBlobRegistry()->RegisterPublicBlobURL(url, handle->Uuid());
}

void BlobRegistry::RevokePublicBlobURL(const KURL& url) {
  RemoveFromOriginMap(url);
  GetBlobRegistry()->RevokePublicBlobURL(url);
}

static void RegisterStreamURLTask(const KURL& url, const String& type) {
  if (WebBlobRegistry* registry = GetBlobRegistry())
    registry->RegisterStreamURL(url, type);
}

void BlobRegistry::RegisterStreamURL(const KURL& url, const String& type) {
  if (IsMainThread())
    RegisterStreamURLTask(url, type);
  else
    Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE, CrossThreadBind(&RegisterStreamURLTask, url, type));
}

static void RegisterStreamURLFromTask(const KURL& url, const KURL& src_url) {
  if (WebBlobRegistry* registry = GetBlobRegistry())
    registry->RegisterStreamURL(url, src_url);
}

void BlobRegistry::RegisterStreamURL(SecurityOrigin* origin,
                                     const KURL& url,
                                     const KURL& src_url) {
  SaveToOriginMap(origin, url);

  if (IsMainThread())
    RegisterStreamURLFromTask(url, src_url);
  else
    Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&RegisterStreamURLFromTask, url, src_url));
}

static void AddDataToStreamTask(const KURL& url,
                                PassRefPtr<RawData> stream_data) {
  if (WebBlobRegistry* registry = GetBlobRegistry())
    registry->AddDataToStream(url, stream_data->Data(), stream_data->length());
}

void BlobRegistry::AddDataToStream(const KURL& url,
                                   PassRefPtr<RawData> stream_data) {
  if (IsMainThread())
    AddDataToStreamTask(url, std::move(stream_data));
  else
    Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&AddDataToStreamTask, url, std::move(stream_data)));
}

static void FlushStreamTask(const KURL& url) {
  if (WebBlobRegistry* registry = GetBlobRegistry())
    registry->FlushStream(url);
}

void BlobRegistry::FlushStream(const KURL& url) {
  if (IsMainThread())
    FlushStreamTask(url);
  else
    Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE, CrossThreadBind(&FlushStreamTask, url));
}

static void FinalizeStreamTask(const KURL& url) {
  if (WebBlobRegistry* registry = GetBlobRegistry())
    registry->FinalizeStream(url);
}

void BlobRegistry::FinalizeStream(const KURL& url) {
  if (IsMainThread())
    FinalizeStreamTask(url);
  else
    Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE, CrossThreadBind(&FinalizeStreamTask, url));
}

static void AbortStreamTask(const KURL& url) {
  if (WebBlobRegistry* registry = GetBlobRegistry())
    registry->AbortStream(url);
}

void BlobRegistry::AbortStream(const KURL& url) {
  if (IsMainThread())
    AbortStreamTask(url);
  else
    Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE, CrossThreadBind(&AbortStreamTask, url));
}

static void UnregisterStreamURLTask(const KURL& url) {
  if (WebBlobRegistry* registry = GetBlobRegistry())
    registry->UnregisterStreamURL(url);
}

void BlobRegistry::UnregisterStreamURL(const KURL& url) {
  RemoveFromOriginMap(url);

  if (IsMainThread())
    UnregisterStreamURLTask(url);
  else
    Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE, CrossThreadBind(&UnregisterStreamURLTask, url));
}

BlobOriginMap::BlobOriginMap() {
  SecurityOrigin::SetMap(this);
}

SecurityOrigin* BlobOriginMap::GetOrigin(const KURL& url) {
  if (url.ProtocolIs("blob"))
    return OriginMap()->at(url.GetString());
  return 0;
}

}  // namespace blink
