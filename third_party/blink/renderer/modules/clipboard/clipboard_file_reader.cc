// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_file_reader.h"

#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"

namespace blink {
ClipboardFileReader::ClipboardFileReader(Blob* blob, ClipboardPromise* promise)
    : loader_(
          FileReaderLoader::Create(FileReaderLoader::kReadAsArrayBuffer, this)),
      promise_(promise) {
  loader_->Start(blob->GetBlobDataHandle());
}

ClipboardFileReader::~ClipboardFileReader() = default;

// FileReaderLoaderClient implementation.
void ClipboardFileReader::DidFinishLoading() {
  DOMArrayBuffer* array_buffer = loader_->ArrayBufferResult();
  promise_->OnLoadComplete(array_buffer);
}

void ClipboardFileReader::DidFail(FileErrorCode error_code) {
  promise_->Reject();
}

}  // namespace blink
