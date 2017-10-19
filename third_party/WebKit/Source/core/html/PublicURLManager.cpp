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
#include "platform/blob/BlobURL.h"
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

String PublicURLManager::RegisterURL(ExecutionContext* context,
                                     URLRegistrable* registrable,
                                     const String& uuid) {
  SecurityOrigin* origin = context->GetSecurityOrigin();
  const KURL& url = BlobURL::CreatePublicURL(origin);
  DCHECK(!url.IsEmpty());
  const String& url_string = url.GetString();

  if (!is_stopped_) {
    RegistryURLMap::ValueType* found =
        registry_to_url_.insert(&registrable->Registry(), URLMap())
            .stored_value;
    found->key->RegisterURL(origin, url, registrable);
    found->value.insert(url_string, uuid);
  }

  return url_string;
}

void PublicURLManager::Revoke(const KURL& url) {
  for (auto& registry_url : registry_to_url_) {
    if (registry_url.value.Contains(url.GetString())) {
      registry_url.key->UnregisterURL(url);
      registry_url.value.erase(url.GetString());
      break;
    }
  }
}

void PublicURLManager::Revoke(const String& uuid) {
  // A linear scan; revoking by UUID is assumed rare.
  Vector<String> urls_to_remove;
  for (auto& registry_url : registry_to_url_) {
    URLRegistry* registry = registry_url.key;
    URLMap& registered_urls = registry_url.value;
    for (auto& registered_url : registered_urls) {
      if (uuid == registered_url.value) {
        KURL url(kParsedURLString, registered_url.key);
        GetExecutionContext()->RemoveURLFromMemoryCache(url);
        registry->UnregisterURL(url);
        urls_to_remove.push_back(registered_url.key);
      }
    }
    for (const auto& url : urls_to_remove)
      registered_urls.erase(url);
    urls_to_remove.clear();
  }
}

void PublicURLManager::ContextDestroyed(ExecutionContext*) {
  if (is_stopped_)
    return;

  is_stopped_ = true;
  for (auto& registry_url : registry_to_url_) {
    for (auto& url : registry_url.value)
      registry_url.key->UnregisterURL(KURL(kParsedURLString, url.key));
  }

  registry_to_url_.clear();
}

void PublicURLManager::Trace(blink::Visitor* visitor) {
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
