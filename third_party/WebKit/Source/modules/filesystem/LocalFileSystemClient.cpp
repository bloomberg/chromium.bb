/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "modules/filesystem/LocalFileSystemClient.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/frame/ContentSettingsClient.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerContentSettingsClient.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/ContentSettingCallbacks.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

std::unique_ptr<FileSystemClient> LocalFileSystemClient::Create() {
  return WTF::WrapUnique(
      static_cast<FileSystemClient*>(new LocalFileSystemClient()));
}

LocalFileSystemClient::~LocalFileSystemClient() {}

bool LocalFileSystemClient::RequestFileSystemAccessSync(
    ExecutionContext* context) {
  DCHECK(context);
  if (context->IsDocument()) {
    NOTREACHED();
    return false;
  }

  DCHECK(context->IsWorkerGlobalScope());
  return WorkerContentSettingsClient::From(*ToWorkerGlobalScope(context))
      ->RequestFileSystemAccessSync();
}

void LocalFileSystemClient::RequestFileSystemAccessAsync(
    ExecutionContext* context,
    std::unique_ptr<ContentSettingCallbacks> callbacks) {
  DCHECK(context);
  if (!context->IsDocument()) {
    NOTREACHED();
    return;
  }

  Document* document = ToDocument(context);
  DCHECK(document->GetFrame());
  document->GetFrame()
      ->GetContentSettingsClient()
      ->RequestFileSystemAccessAsync(std::move(callbacks));
}

LocalFileSystemClient::LocalFileSystemClient() {}

}  // namespace blink
