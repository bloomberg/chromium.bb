// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_FILE_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_FILE_READER_H_

#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader_client.h"

namespace blink {

class FileReaderLoader;
class ClipboardPromise;

// Single-use object for reading a Blob or File asynchronously.
//
// Takes in a Blob* and ClipboardPromise, and outputs the contents of the Blob
// back to the ClipboardPromise.
//
// Created for the intent of use with clipboard, but may be generic enough for
// other uses.
//
// TODO (crbug.com/916821): This class is very similar to ImageBitmapFactories::
// ImageBitmapLoader. Ask ImageBitmapLoader creators if there's potential to
// merge code and reduce duplicate code.
class ClipboardFileReader final : public FileReaderLoaderClient {
 public:
  ClipboardFileReader(Blob*, ClipboardPromise*);
  ~ClipboardFileReader() override;

  // FileReaderLoaderClient.
  void DidStartLoading() override {}
  void DidReceiveData() override {}
  void DidFinishLoading() override;
  void DidFail(FileErrorCode) override;

 private:
  // This FileReaderLoader will load the Blob.
  const std::unique_ptr<FileReaderLoader> loader_;
  // This ClipboardPromise owns this loader.
  Persistent<ClipboardPromise> promise_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_FILE_READER_H_
