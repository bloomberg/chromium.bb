/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

#include "core/fileapi/FileReaderSync.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileError.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/frame/Deprecation.h"
#include "platform/Histogram.h"

namespace blink {

namespace {
// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class WorkerType {
  OTHER = 0,
  DEDICATED_WORKER = 1,
  SHARED_WORKER = 2,
  SERVICE_WORKER = 3,
  MAX
};
}  // namespace

FileReaderSync::FileReaderSync(ExecutionContext* context) {
  if (context->isServiceWorkerGlobalScope()) {
    Deprecation::countDeprecation(context,
                                  UseCounter::FileReaderSyncInServiceWorker);
  }

  WorkerType type = WorkerType::OTHER;
  if (context->isDedicatedWorkerGlobalScope())
    type = WorkerType::DEDICATED_WORKER;
  else if (context->isSharedWorkerGlobalScope())
    type = WorkerType::SHARED_WORKER;
  else if (context->isServiceWorkerGlobalScope())
    type = WorkerType::SERVICE_WORKER;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, workerTypeHistogram,
      new EnumerationHistogram("FileReaderSync.WorkerType",
                               static_cast<int>(WorkerType::MAX)));
  workerTypeHistogram.count(static_cast<int>(type));
}

DOMArrayBuffer* FileReaderSync::readAsArrayBuffer(
    ScriptState* scriptState,
    Blob* blob,
    ExceptionState& exceptionState) {
  ASSERT(blob);

  std::unique_ptr<FileReaderLoader> loader =
      FileReaderLoader::create(FileReaderLoader::ReadAsArrayBuffer, nullptr);
  startLoading(scriptState->getExecutionContext(), *loader, *blob,
               exceptionState);

  return loader->arrayBufferResult();
}

String FileReaderSync::readAsBinaryString(ScriptState* scriptState,
                                          Blob* blob,
                                          ExceptionState& exceptionState) {
  ASSERT(blob);

  std::unique_ptr<FileReaderLoader> loader =
      FileReaderLoader::create(FileReaderLoader::ReadAsBinaryString, nullptr);
  startLoading(scriptState->getExecutionContext(), *loader, *blob,
               exceptionState);
  return loader->stringResult();
}

String FileReaderSync::readAsText(ScriptState* scriptState,
                                  Blob* blob,
                                  const String& encoding,
                                  ExceptionState& exceptionState) {
  ASSERT(blob);

  std::unique_ptr<FileReaderLoader> loader =
      FileReaderLoader::create(FileReaderLoader::ReadAsText, nullptr);
  loader->setEncoding(encoding);
  startLoading(scriptState->getExecutionContext(), *loader, *blob,
               exceptionState);
  return loader->stringResult();
}

String FileReaderSync::readAsDataURL(ScriptState* scriptState,
                                     Blob* blob,
                                     ExceptionState& exceptionState) {
  ASSERT(blob);

  std::unique_ptr<FileReaderLoader> loader =
      FileReaderLoader::create(FileReaderLoader::ReadAsDataURL, nullptr);
  loader->setDataType(blob->type());
  startLoading(scriptState->getExecutionContext(), *loader, *blob,
               exceptionState);
  return loader->stringResult();
}

void FileReaderSync::startLoading(ExecutionContext* executionContext,
                                  FileReaderLoader& loader,
                                  const Blob& blob,
                                  ExceptionState& exceptionState) {
  loader.start(executionContext, blob.blobDataHandle());
  if (loader.errorCode())
    FileError::throwDOMException(exceptionState, loader.errorCode());
}

}  // namespace blink
