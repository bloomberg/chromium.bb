/*
 * Copyright (C) 2012 Motorola Mobility Inc.
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
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

#include "core/html/PublicURLManager.h"

#include "core/html/URLRegistry.h"
#include "platform/blob/BlobData.h"
#include "platform/blob/BlobURL.h"
#include "platform/runtime_enabled_features.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

class SecurityOrigin;

PublicURLManager* PublicURLManager::Create(ExecutionContext* context) {
  return new PublicURLManager(context);
}

PublicURLManager::PublicURLManager(ExecutionContext* context)
    : ContextLifecycleObserver(context), is_stopped_(false) {}

String PublicURLManager::RegisterURL(URLRegistrable* registrable) {
  if (is_stopped_)
    return String();

  SecurityOrigin* origin = GetExecutionContext()->GetMutableSecurityOrigin();
  const KURL& url = BlobURL::CreatePublicURL(origin);
  DCHECK(!url.IsEmpty());
  const String& url_string = url.GetString();

  mojom::blink::BlobPtr blob;
  if (RuntimeEnabledFeatures::MojoBlobURLsEnabled())
    blob = registrable->AsMojoBlob();
  if (blob) {
    if (!url_store_) {
      BlobDataHandle::GetBlobRegistry()->URLStoreForOrigin(
          origin, MakeRequest(&url_store_));
    }
    url_store_->Register(std::move(blob), url);
  } else {
    URLRegistry* registry = &registrable->Registry();
    registry->RegisterURL(origin, url, registrable);
    url_to_registry_.insert(url_string, registry);
  }

  return url_string;
}

void PublicURLManager::Revoke(const KURL& url) {
  if (RuntimeEnabledFeatures::MojoBlobURLsEnabled() && !is_stopped_) {
    if (!url_store_) {
      BlobDataHandle::GetBlobRegistry()->URLStoreForOrigin(
          GetExecutionContext()->GetSecurityOrigin(), MakeRequest(&url_store_));
    }
    url_store_->Revoke(url);
  }
  auto it = url_to_registry_.find(url.GetString());
  if (it == url_to_registry_.end())
    return;
  it->value->UnregisterURL(url);
  url_to_registry_.erase(it);
}

void PublicURLManager::ContextDestroyed(ExecutionContext*) {
  if (is_stopped_)
    return;

  is_stopped_ = true;
  for (auto& url_registry : url_to_registry_)
    url_registry.value->UnregisterURL(KURL(url_registry.key));

  url_to_registry_.clear();

  url_store_.reset();
}

void PublicURLManager::Trace(blink::Visitor* visitor) {
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
