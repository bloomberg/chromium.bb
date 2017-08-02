/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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

#include "platform/FileMetadata.h"

#include "public/platform/Platform.h"
#include "public/platform/WebFileInfo.h"
#include "public/platform/WebFileUtilities.h"

namespace blink {

bool GetFileSize(const String& path, long long& result) {
  FileMetadata metadata;
  if (!GetFileMetadata(path, metadata))
    return false;
  result = metadata.length;
  return true;
}

bool GetFileModificationTime(const String& path, double& result) {
  FileMetadata metadata;
  if (!GetFileMetadata(path, metadata))
    return false;
  result = metadata.modification_time;
  return true;
}

bool GetFileMetadata(const String& path, FileMetadata& metadata) {
  WebFileInfo web_file_info;
  if (!Platform::Current()->GetFileUtilities()->GetFileInfo(path,
                                                            web_file_info))
    return false;
  metadata.modification_time = web_file_info.modification_time;
  metadata.length = web_file_info.length;
  metadata.type = static_cast<FileMetadata::Type>(web_file_info.type);
  return true;
}

String DirectoryName(const String& path) {
  return Platform::Current()->GetFileUtilities()->DirectoryName(path);
}

KURL FilePathToURL(const String& path) {
  return Platform::Current()->GetFileUtilities()->FilePathToURL(path);
}

STATIC_ASSERT_ENUM(WebFileInfo::kTypeUnknown, FileMetadata::kTypeUnknown);
STATIC_ASSERT_ENUM(WebFileInfo::kTypeFile, FileMetadata::kTypeFile);
STATIC_ASSERT_ENUM(WebFileInfo::kTypeDirectory, FileMetadata::kTypeDirectory);

}  // namespace blink
