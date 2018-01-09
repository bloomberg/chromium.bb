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

#ifndef AsyncFileSystemCallbacks_h
#define AsyncFileSystemCallbacks_h

#include <memory>
#include "platform/FileMetadata.h"
#include "platform/FileSystemType.h"
#include "platform/blob/BlobData.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebFileWriter.h"

namespace blink {

class PLATFORM_EXPORT AsyncFileSystemCallbacks {
  USING_FAST_MALLOC(AsyncFileSystemCallbacks);
  WTF_MAKE_NONCOPYABLE(AsyncFileSystemCallbacks);

 public:
  AsyncFileSystemCallbacks() : block_until_completion_(false) {}

  // Called when a requested operation is completed successfully.
  virtual void DidSucceed() { NOTREACHED(); }

  // Called when a requested file system is opened.
  virtual void DidOpenFileSystem(const String& name, const KURL& root_url) {
    NOTREACHED();
  }

  // Called when a filesystem URL is resolved.
  virtual void DidResolveURL(const String& name,
                             const KURL& root_url,
                             FileSystemType,
                             const String& file_path,
                             bool is_directory) {
    NOTREACHED();
  }

  // Called when a file metadata is read successfully.
  virtual void DidReadMetadata(const FileMetadata&) { NOTREACHED(); }

  // Called when a snapshot file is created successfully.
  virtual void DidCreateSnapshotFile(const FileMetadata&,
                                     scoped_refptr<BlobDataHandle> snapshot) {
    NOTREACHED();
  }

  // Called when a directory entry is read.
  virtual void DidReadDirectoryEntry(const String& name, bool is_directory) {
    NOTREACHED();
  }

  // Called after a chunk of directory entries have been read (i.e. indicates
  // it's good time to call back to the application). If hasMore is true there
  // can be more chunks.
  virtual void DidReadDirectoryEntries(bool has_more) { NOTREACHED(); }

  // Called when an AsyncFileWrter has been created successfully.
  virtual void DidCreateFileWriter(std::unique_ptr<WebFileWriter>,
                                   long long length) {
    NOTREACHED();
  }

  // Called when there was an error.
  virtual void DidFail(int code) = 0;

  // Returns true if the caller expects that the calling thread blocks
  // until completion.
  virtual bool ShouldBlockUntilCompletion() const {
    return block_until_completion_;
  }

  void SetShouldBlockUntilCompletion(bool flag) {
    block_until_completion_ = flag;
  }

  virtual ~AsyncFileSystemCallbacks() = default;

 private:
  bool block_until_completion_;
};

}  // namespace blink

#endif  // AsyncFileSystemCallbacks_h
