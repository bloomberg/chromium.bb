/*
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/storage/StorageClient.h"

#include <memory>
#include "core/exported/WebViewBase.h"
#include "core/frame/ContentSettingsClient.h"
#include "modules/storage/StorageNamespace.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebStorageNamespace.h"
#include "public/web/WebViewClient.h"

namespace blink {

#define STATIC_ASSERT_MATCHING_ENUM(enum_name1, enum_name2)                   \
  static_assert(static_cast<int>(enum_name1) == static_cast<int>(enum_name2), \
                "mismatching enums: " #enum_name1)
STATIC_ASSERT_MATCHING_ENUM(kLocalStorage,
                            ContentSettingsClient::StorageType::kLocal);
STATIC_ASSERT_MATCHING_ENUM(kSessionStorage,
                            ContentSettingsClient::StorageType::kSession);

StorageClient::StorageClient(WebViewBase* web_view) : web_view_(web_view) {}

std::unique_ptr<StorageNamespace>
StorageClient::CreateSessionStorageNamespace() {
  if (!web_view_->Client())
    return nullptr;
  return WTF::WrapUnique(new StorageNamespace(
      WTF::WrapUnique(web_view_->Client()->CreateSessionStorageNamespace())));
}

bool StorageClient::CanAccessStorage(LocalFrame* frame,
                                     StorageType type) const {
  DCHECK(frame->GetContentSettingsClient());
  return frame->GetContentSettingsClient()->AllowStorage(
      static_cast<ContentSettingsClient::StorageType>(type));
}

}  // namespace blink
